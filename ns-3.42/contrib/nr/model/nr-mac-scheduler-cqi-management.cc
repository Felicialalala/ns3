// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_getCellId)                                                                               \
    {                                                                                              \
        std::clog << " [ CellId " << GetCellId() << ", bwpId " << GetBwpId() << "] ";              \
    }
#include "nr-mac-scheduler-cqi-management.h"

#include "nr-amc.h"

#include <ns3/log.h>
#include <ns3/nr-spectrum-value-helper.h>
#include <fstream>
#include <sstream>
#include <ns3/recentbler.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMacSchedulerCQIManagement");

// void
// NrMacSchedulerCQIManagement::DlSBCQIReported(
//     [[maybe_unused]] const DlCqiInfo& info,
//     [[maybe_unused]] const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo) const
// {
//     NS_LOG_FUNCTION(this);
//     // TODO
//     NS_ABORT_MSG("SB CQI Type is not supported");
// }

// ns-3.42/src/lte/model/nr-mac-scheduler-cqi-management.cc
#include <vector>
#include <string>

std::map<uint16_t, std::map<uint16_t, RecentBler>> recentBlerMap;

void
NrMacSchedulerCQIManagement::DlSBCQIReported(const DlCqiInfo& info,
                                             const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                             uint32_t expirationTime,
                                             int8_t maxDlMcs,
                                             const Ptr<const SpectrumModel>& model) const
{
    NS_LOG_FUNCTION(this);

    // Set the CQI type and update the CQI information
    ueInfo->m_dlCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::SB;
    ueInfo->m_dlCqi.m_wbCqi = info.m_wbCqi;
    ueInfo->m_dlCqi.m_timer = expirationTime;

    // Extract SINR values from info.m_sinr
    std::vector<double> sinrValues(info.m_sinr.GetValuesN());
    for (size_t i = 0; i < info.m_sinr.GetValuesN(); ++i)
    {
        sinrValues[i] = info.m_sinr[i];
    }

    // Find the minimum non-zero SINR value
    double minSinr = std::numeric_limits<double>::max();
    for (double sinr : sinrValues)
    {
        if (sinr > 0.0 && sinr < minSinr)
        {
            minSinr = sinr;
        }
    }

    // If no non-zero SINR value is found, set minSinr to 0
    if (minSinr == std::numeric_limits<double>::max())
    {
        minSinr = 0.0;
    }

    // Create rbAssignment vector: first 10 elements are minSinr, rest are 0
    std::vector<double> rbAssignment(sinrValues.size(), 0.0);
    for (size_t i = 0; i < std::min(30ul, sinrValues.size()); ++i)
    {
        rbAssignment[i] = minSinr;
    }

    // Create a SpectrumValue object from the rbAssignment vector
    SpectrumValue specVals(model);
    for (size_t i = 0; i < rbAssignment.size(); ++i)
    {
        specVals[i] = rbAssignment[i];
    }

    // Calculate the MCS using the AMC module, which will also calculate the tbler
    ueInfo->m_dlCqi.m_wbCqi = GetAmcDl()->CreateCqiFeedbackWbTdma(specVals, ueInfo->m_dlMcs);

    NS_LOG_UNCOND("Calculated MCS for UE "<< this->GetCellId() <<": " << ueInfo->m_rnti << " is " << ueInfo->m_dlCqi.m_wbCqi
                                           << " with tbler " << ueInfo->m_dlCqi.m_tbler);

    NS_LOG_UNCOND("Calculated MCS for UE " << ueInfo->m_rnti << " is "
                  << static_cast<uint32_t>(ueInfo->m_dlMcs));
    NS_LOG_UNCOND("Updated WB CQI of UE "
                  << ueInfo->m_rnti << " to " << static_cast<uint32_t>(ueInfo->m_dlCqi.m_wbCqi)
                  << ". It will expire in " << ueInfo->m_dlCqi.m_timer << " slots.");

    if (info.m_optPrecMat)
    {
        // Set the number of layers (rank) directly to m_ri (do not decode m_ri)
        NS_ASSERT(info.m_ri > 0);
        ueInfo->m_dlRank = info.m_ri;
        // Set the precoding matrix
        ueInfo->m_dlPrecMats = info.m_optPrecMat;
    }
}

//UlSBCQIReported 是 NrMacSchedulerCQIManagement 类中的一个常量成员函数，用于处理上行子带（SB）信道质量指示（CQI）报告。
//该函数根据接收到的参数更新用户设备（UE）的上行 CQI 信息，并计算宽带（WB）CQI 和调制与编码策略（MCS）。
// void
// NrMacSchedulerCQIManagement::UlSBCQIReported(
//     uint32_t expirationTime,// CQI 信息的过期时间
//     [[maybe_unused]] uint32_t tbs,
//     const NrMacSchedSapProvider::SchedUlCqiInfoReqParameters& params,//NrMacSchedSapProvider::SchedUlCqiInfoReqParameters 类型的常量引用，包含上行 CQI 报告的相关参数
//     const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,//std::shared_ptr<NrMacSchedulerUeInfo> 类型，指向 UE 信息的智能指针
//     const std::vector<uint8_t>& rbgMask,//资源块组（RBG）掩码，用于指示哪些 RBG 被分配给 UE
//     uint32_t numRbPerRbg,
//     const Ptr<const SpectrumModel>& model) const//Ptr<const SpectrumModel> 类型的常量引用，指向频谱模型的指针
// {
//     NS_LOG_FUNCTION(this);
//     NS_ASSERT(!rbgMask.empty());

//     NS_LOG_INFO("Computing SB CQI for UE " << ueInfo->m_rnti);

//     ueInfo->m_ulCqi.m_sinr = params.m_ulCqi.m_sinr;//将 params 中的上行 SINR 值赋给 UE 的上行 CQI 信息
//     ueInfo->m_ulCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::SB;//设置 UE 的上行 CQI 类型为子带 CQI
//     ueInfo->m_ulCqi.m_timer = expirationTime;//设置 UE 的上行 CQI 定时器为过期时间

//     //生成资源块分配向量
//     //初始化一个长度为 params.m_ulCqi.m_sinr.size() 的向量 rbAssignment，并将所有元素初始化为 0
//     std::vector<int> rbAssignment(params.m_ulCqi.m_sinr.size(), 0);
//     //遍历 rbgMask，如果某个 RBG 被分配（值为 1），则将该 RBG 包含的所有 RB 在 rbAssignment 中对应的元素设置为 1
//     for (uint32_t i = 0; i < rbgMask.size(); ++i)
//     {
//         if (rbgMask.at(i) == 1)
//         {
//             for (uint32_t k = 0; k < numRbPerRbg; ++k)
//             {
//                 rbAssignment[i * numRbPerRbg + k] = 1;
//             }
//         }
//     }

//     // 生成频谱值对象
//     // 创建一个 SpectrumValue 对象 specVals，并使用频谱模型 model 进行初始化。
//     SpectrumValue specVals(model);
//     Values::iterator specIt = specVals.ValuesBegin();

//     std::stringstream out;

//     // 遍历频谱模型的每个频段，根据 rbAssignment 向量的值，将对应的 SINR 值赋给 specVals，未分配的 RB 对应的 SINR 值设置为 0.0
//     for (uint32_t ichunk = 0; ichunk < model->GetNumBands(); ichunk++)
//     {
//         NS_ASSERT(specIt != specVals.ValuesEnd());
//         if (rbAssignment[ichunk] == 1)//某个频段对应的资源块被分配给了 UE
//         {
//             //将该频段的 SINR 值从 ueInfo->m_ulCqi.m_sinr 中取出并赋给 specVals 中对应的元素
//             *specIt = ueInfo->m_ulCqi.m_sinr.at(ichunk);
//             out << ueInfo->m_ulCqi.m_sinr.at(ichunk) << " ";
//         }
//         else
//         {
//             out << "0.0 ";
//             *specIt = 0.0;
//         }

//         specIt++;
//     }

//     //记录要传递给自适应调制和编码（AMC）模块的 SINR 值。真正要传递给 AMC 模块的 SINR 值存储在 SpectrumValue 对象 specVals 中
//     NS_LOG_INFO("Values of SINR to pass to the AMC: " << out.str());

//     // MCS updated inside the function; crappy API... but we can't fix everything
//     ueInfo->m_ulCqi.m_wbCqi = GetAmcUl()->CreateCqiFeedbackWbTdma(specVals, ueInfo->m_ulMcs);
//     NS_LOG_DEBUG("Calculated MCS for RNTI " << ueInfo->m_rnti << " is " << ueInfo->m_ulMcs);
// }

void
NrMacSchedulerCQIManagement::UlSBCQIReported(uint32_t expirationTime,// CQI 信息的过期时间
    [[maybe_unused]] uint32_t tbs,
    const NrMacSchedSapProvider::SchedUlCqiInfoReqParameters& params,//NrMacSchedSapProvider::SchedUlCqiInfoReqParameters 类型的常量引用，包含上行 CQI 报告的相关参数
    const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,//std::shared_ptr<NrMacSchedulerUeInfo> 类型，指向 UE 信息的智能指针
    const std::vector<uint8_t>& rbgMask,//资源块组（RBG）掩码，用于指示哪些 RBG 被分配给 UE
    uint32_t numRbPerRbg,
    const Ptr<const SpectrumModel>& model) const//Ptr<const SpectrumModel> 类型的常量引用，指向频谱模型的指针
{
    ueInfo->m_ulCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::SB;//设置 UE 的上行 CQI 类型为子带 CQI
    ueInfo->m_ulCqi.m_timer = expirationTime;//设置 UE 的上行 CQI 定时器为过期时间

    //NS_LOG_UNCOND("old mcs from cqi: "<< static_cast<int>(ueInfo->m_ulMcs));

    // ============== 新增BLER自适应逻辑 ==============
    uint16_t cellId = ueInfo->m_cellId;
    uint16_t rnti = ueInfo->m_rnti;
    
    // 1. 获取当前UE的RecentBler
    double currentBler = 0.0;
    if (recentBlerMap.find(cellId) != recentBlerMap.end() &&
        recentBlerMap[cellId].find(rnti) != recentBlerMap[cellId].end()) 
    {
        currentBler = recentBlerMap[cellId][rnti].GetBler();
        NS_LOG_UNCOND("UE " << rnti << " in Cell " << cellId 
                       << " current BLER: " << currentBler*100 << "%");
    }
    else 
    {
        NS_LOG_WARN("No BLER data for UE " << rnti << " in Cell " << cellId);
    }

    // 2. 根据BLER调整MCS
    uint8_t originalMcs = ueInfo->m_ulMcs;

    // 改为动态目标
    double targetBler = 0.1;  // 根据系统负载动态调整

    // 建议改为动态步长（示例）
    double blerError = currentBler - targetBler;
    int delta = static_cast<int>(std::round(blerError * 10));  // 每10%误差对应1步调整
    delta = std::clamp(delta, -2, 2);  // 限制最大调整幅度

    if (currentBler > targetBler ) {
        ueInfo->m_ulMcs = std::max(0, static_cast<int>(ueInfo->m_ulMcs) - delta);
        NS_LOG_INFO("High BLER (" << currentBler*100 << "%), reducing MCS from " 
                     << static_cast<uint32_t>(originalMcs) << " to " 
                     << static_cast<uint32_t>(ueInfo->m_ulMcs));
    } else if (currentBler < 0.02) {
        // 调整策略
        ueInfo->m_ulMcs = std::min(static_cast<int>(27), 
                                  static_cast<int>(ueInfo->m_ulMcs) - delta);
        NS_LOG_INFO("ueInfo->m_ulMcs + 1 : "<< static_cast<int>(ueInfo->m_ulMcs) + 1);
        NS_LOG_INFO("Low BLER (" << currentBler*100 << "%), increasing MCS from " 
                     << static_cast<uint32_t>(originalMcs) << " to " 
                     << static_cast<uint32_t>(ueInfo->m_ulMcs));
    }

    // 3. 确保MCS在有效范围内
    ueInfo->m_ulMcs = std::clamp(ueInfo->m_ulMcs, 
        static_cast<uint8_t>(0), 
        static_cast<uint8_t>(27));

    // ============== 原有逻辑继续执行 ==============
    NS_LOG_INFO("Final MCS for UE " <<"cell: "<< cellId <<" - "<< rnti << " is "
    << static_cast<uint32_t>(ueInfo->m_ulMcs));


    NS_LOG_INFO("Calculated MCS for UE " <<"cell: "<<this->GetCellId()<<" - "<< ueInfo->m_rnti << " is "
                    << static_cast<uint32_t>(ueInfo->m_ulMcs));

    NS_LOG_INFO("Updated WB CQI of UE "
    << ueInfo->m_rnti << " to " << static_cast<uint32_t>(ueInfo->m_ulCqi.m_wbCqi)
    << ". It will expire in " << ueInfo->m_ulCqi.m_timer << " slots.");

     

}

void
NrMacSchedulerCQIManagement::InstallGetBwpIdFn(const std::function<uint16_t()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getBwpId = fn;
}

void
NrMacSchedulerCQIManagement::InstallGetCellIdFn(const std::function<uint16_t()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getCellId = fn;
}

void
NrMacSchedulerCQIManagement::InstallGetStartMcsDlFn(const std::function<uint8_t()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getStartMcsDl = fn;
}

void
NrMacSchedulerCQIManagement::InstallGetStartMcsUlFn(const std::function<uint8_t()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getStartMcsUl = fn;
}

void
NrMacSchedulerCQIManagement::InstallGetNrAmcDlFn(const std::function<Ptr<const NrAmc>()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getAmcDl = fn;
}

void
NrMacSchedulerCQIManagement::InstallGetNrAmcUlFn(const std::function<Ptr<const NrAmc>()>& fn)
{
    NS_LOG_FUNCTION(this);
    m_getAmcUl = fn;
}

void
NrMacSchedulerCQIManagement:: DlWBCQIReported(const DlCqiInfo& info,
                                             const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                             uint32_t expirationTime,
                                             int8_t maxDlMcs) const
{
    NS_LOG_FUNCTION(this);

    ueInfo->m_dlCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::WB;
    ueInfo->m_dlCqi.m_wbCqi = info.m_wbCqi;
    ueInfo->m_dlCqi.m_timer = expirationTime;
    
    
    //旧逻辑，没有根据bler调节mcs
    // ueInfo->m_dlMcs =
    //     std::min(static_cast<uint8_t>(GetAmcDl()->GetMcsFromCqi(ueInfo->m_dlCqi.m_wbCqi)),
    //              static_cast<uint8_t>(maxDlMcs));
    NS_LOG_UNCOND("old mcs from cqi: "<< static_cast<int>(ueInfo->m_dlMcs));

    // ============== 新增BLER自适应逻辑 ==============
    uint16_t cellId = ueInfo->m_cellId;
    uint16_t rnti = ueInfo->m_rnti;
    
    // 1. 获取当前UE的RecentBler
    double currentBler = 0.0;
    if (recentBlerMap.find(cellId) != recentBlerMap.end() &&
        recentBlerMap[cellId].find(rnti) != recentBlerMap[cellId].end()) 
    {
        currentBler = recentBlerMap[cellId][rnti].GetBler();
        NS_LOG_UNCOND("UE " << rnti << " in Cell " << cellId 
                       << " current BLER: " << currentBler*100 << "%");
    }
    else 
    {
        NS_LOG_WARN("No BLER data for UE " << rnti << " in Cell " << cellId);
    }

    // 2. 根据BLER调整MCS
    uint8_t originalMcs = ueInfo->m_dlMcs;

    // 改为动态目标
    double targetBler = 0.1;  // 根据系统负载动态调整

    // 建议改为动态步长（示例）
    double blerError = currentBler - targetBler;
    int delta = static_cast<int>(std::round(blerError * 10));  // 每10%误差对应1步调整
    delta = std::clamp(delta, -2, 2);  // 限制最大调整幅度

    if (currentBler > targetBler ) {
        ueInfo->m_dlMcs = std::max(0, static_cast<int>(ueInfo->m_dlMcs) - delta);
        NS_LOG_UNCOND("High BLER (" << currentBler*100 << "%), reducing MCS from " 
                     << static_cast<uint32_t>(originalMcs) << " to " 
                     << static_cast<uint32_t>(ueInfo->m_dlMcs));
    } else if (currentBler < 0.02) {
        // 调整策略
        ueInfo->m_dlMcs = std::min(static_cast<int>(maxDlMcs), 
                                  static_cast<int>(ueInfo->m_dlMcs) - delta);
        NS_LOG_UNCOND("maxDlMcs: "<< static_cast<int>((maxDlMcs)));
        NS_LOG_UNCOND("ueInfo->m_dlMcs + 1 : "<< static_cast<int>(ueInfo->m_dlMcs) + 1);
        NS_LOG_UNCOND("Low BLER (" << currentBler*100 << "%), increasing MCS from " 
                     << static_cast<uint32_t>(originalMcs) << " to " 
                     << static_cast<uint32_t>(ueInfo->m_dlMcs));
    }
    
    // // BLER阈值策略
    // if (currentBler > 0.10) // BLER > 10%
    // {
    //     ueInfo->m_dlMcs = std::max(0, static_cast<int>(ueInfo->m_dlMcs) - 1);
    //     NS_LOG_UNCOND("High BLER (" << currentBler*100 << "%), reducing MCS from " 
    //                  << static_cast<uint32_t>(originalMcs) << " to " 
    //                  << static_cast<uint32_t>(ueInfo->m_dlMcs));
    // }
    // else if (currentBler < 0.02) // BLER < 2%
    // {
    //     ueInfo->m_dlMcs = std::min(static_cast<int>(maxDlMcs), 
    //                               static_cast<int>(ueInfo->m_dlMcs) + 1);
    //     NS_LOG_UNCOND("maxDlMcs: "<< static_cast<int>((maxDlMcs)));
    //     NS_LOG_UNCOND("ueInfo->m_dlMcs + 1 : "<< static_cast<int>(ueInfo->m_dlMcs) + 1);
    //     NS_LOG_UNCOND("Low BLER (" << currentBler*100 << "%), increasing MCS from " 
    //                  << static_cast<uint32_t>(originalMcs) << " to " 
    //                  << static_cast<uint32_t>(ueInfo->m_dlMcs));
    //}

    // 3. 确保MCS在有效范围内
    ueInfo->m_dlMcs = std::clamp(ueInfo->m_dlMcs, 
                                static_cast<uint8_t>(0), 
                                static_cast<uint8_t>(maxDlMcs));
    
    // ============== 原有逻辑继续执行 ==============
    NS_LOG_UNCOND("Final MCS for UE " <<"cell: "<< cellId <<" - "<< rnti << " is "
                     << static_cast<uint32_t>(ueInfo->m_dlMcs));


    NS_LOG_UNCOND("Calculated MCS for UE " <<"cell: "<<this->GetCellId()<<" - "<< ueInfo->m_rnti << " is "
                                         << static_cast<uint32_t>(ueInfo->m_dlMcs));

    NS_LOG_INFO("Updated WB CQI of UE "
                << ueInfo->m_rnti << " to " << static_cast<uint32_t>(ueInfo->m_dlCqi.m_wbCqi)
                << ". It will expire in " << ueInfo->m_dlCqi.m_timer << " slots.");

    if (info.m_optPrecMat)
    {
        // Set the number of layers (rank) directly to m_ri (do not decode m_ri)
        NS_ASSERT(info.m_ri > 0);
        ueInfo->m_dlRank = info.m_ri;

        // Set the precoding matrix
        ueInfo->m_dlPrecMats = info.m_optPrecMat;
    }
}

void
NrMacSchedulerCQIManagement::RefreshDlCqiMaps(
    const std::unordered_map<uint16_t, std::shared_ptr<NrMacSchedulerUeInfo>>& ueMap) const
{
    NS_LOG_FUNCTION(this);

    for (const auto& itUe : ueMap)
    {
        const std::shared_ptr<NrMacSchedulerUeInfo>& ue = itUe.second;

        if (ue->m_dlCqi.m_timer == 0)
        {
            ue->m_dlCqi.m_wbCqi = 1; // lowest value for trying a transmission
            ue->m_dlCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::WB;
            ue->m_dlMcs = GetStartMcsDl();
        }
        else
        {
            ue->m_dlCqi.m_timer -= 1;
        }
    }
}

void
NrMacSchedulerCQIManagement::RefreshUlCqiMaps(
    const std::unordered_map<uint16_t, std::shared_ptr<NrMacSchedulerUeInfo>>& ueMap) const
{
    NS_LOG_FUNCTION(this);

    for (const auto& itUe : ueMap)
    {
        const std::shared_ptr<NrMacSchedulerUeInfo>& ue = itUe.second;

        if (ue->m_ulCqi.m_timer == 0)
        {
            ue->m_ulCqi.m_wbCqi = 1; // lowest value for trying a transmission
            ue->m_ulCqi.m_cqiType = NrMacSchedulerUeInfo::CqiInfo::WB;
            ue->m_ulMcs = GetStartMcsUl();
        }
        else
        {
            ue->m_ulCqi.m_timer -= 1;
        }
    }
}

uint16_t
NrMacSchedulerCQIManagement::GetBwpId() const
{
    return m_getBwpId();
}

uint16_t
NrMacSchedulerCQIManagement::GetCellId() const
{
    return m_getCellId();
}

uint8_t
NrMacSchedulerCQIManagement::GetStartMcsDl() const
{
    return m_getStartMcsDl();
}

uint8_t
NrMacSchedulerCQIManagement::GetStartMcsUl() const
{
    return m_getStartMcsUl();
}

Ptr<const NrAmc>
NrMacSchedulerCQIManagement::GetAmcDl() const
{
    return m_getAmcDl();
}

Ptr<const NrAmc>
NrMacSchedulerCQIManagement::GetAmcUl() const
{
    return m_getAmcUl();
}

} // namespace ns3
