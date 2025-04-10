// Copyright (c) 2024 Your Name/Organization
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-mac-scheduler-ofdma-dp.h"
#include "nr-mac-scheduler-ue-info-dp.h"
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("NrMacSchedulerOfdmaDP");
NS_OBJECT_ENSURE_REGISTERED(NrMacSchedulerOfdmaDP);

TypeId
NrMacSchedulerOfdmaDP::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrMacSchedulerOfdmaDP")
                            .SetParent<NrMacSchedulerOfdmaPF>()
                            .AddConstructor<NrMacSchedulerOfdmaDP>()
                            .AddAttribute("Beta",
                                          "Weight coefficient for delay priority",
                                          DoubleValue(0.1),
                                          MakeDoubleAccessor(&NrMacSchedulerOfdmaDP::m_beta),
                                          MakeDoubleChecker<double>(0, 1));
    return tid;
}

NrMacSchedulerOfdmaDP::NrMacSchedulerOfdmaDP()
{
}

std::shared_ptr<NrMacSchedulerUeInfo>
NrMacSchedulerOfdmaDP::CreateUeRepresentation(
    const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const
{
    // 使用父类的公共接口获取 m_alpha
    double alpha = GetFairnessIndex();
    return std::make_shared<NrMacSchedulerUeInfoDP>(
        alpha, params.m_rnti, params.m_beamId, params.m_cellId,
        std::bind(&NrMacSchedulerOfdmaDP::GetNumRbPerRbg, this));
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&, const NrMacSchedulerNs3::UePtrAndBufferReq&)>
NrMacSchedulerOfdmaDP::GetUeCompareDlFn() const
{
    return NrMacSchedulerUeInfoDP::CompareUeWeightsDl;
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&, const NrMacSchedulerNs3::UePtrAndBufferReq&)>
NrMacSchedulerOfdmaDP::GetUeCompareUlFn() const
{
    return NrMacSchedulerUeInfoDP::CompareUeWeightsUl;
}

void
NrMacSchedulerOfdmaDP::AssignedDlResources(const UePtrAndBufferReq& ue,
                                           const FTResources& assigned,
                                           const FTResources& totAssigned) const
{
    NrMacSchedulerOfdmaPF::AssignedDlResources(ue, assigned, totAssigned);
    auto ueInfo = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDP>(ue.first);
    if (ueInfo)
    {
        ueInfo->ResetDlWaitTime();
        NS_LOG_DEBUG("Reset DL wait time for UE " << ueInfo->m_rnti);
    }
}

void
NrMacSchedulerOfdmaDP::AssignedUlResources(const UePtrAndBufferReq& ue,
                                           const FTResources& assigned,
                                           const FTResources& totAssigned) const
{
    NrMacSchedulerOfdmaPF::AssignedUlResources(ue, assigned, totAssigned);
    auto ueInfo = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDP>(ue.first);
    if (ueInfo)
    {
        ueInfo->ResetUlWaitTime();
        NS_LOG_DEBUG("Reset UL wait time for UE " << ueInfo->m_rnti);
    }
}

void
NrMacSchedulerOfdmaDP::DoSchedDlTriggerReq(const NrMacSchedSapProvider::SchedDlTriggerReqParameters& params)
{
     const auto& ueMap = GetUeMap(); // 使用父类提供的接口获取 m_ueMap
    // 更新所有UE的下行等待时间
    for (auto& ueEntry : ueMap)
    {
        auto ueInfo = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDP>(ueEntry.second);
        if (ueInfo)
        {
            ueInfo->IncrementDlWaitTime();
            NS_LOG_DEBUG("UE " << ueInfo->m_rnti << " DL wait time: " << ueInfo->GetDlWaitTime());
        }
    }
    NrMacSchedulerOfdmaPF::DoSchedDlTriggerReq(params);
}

void
NrMacSchedulerOfdmaDP::DoSchedUlTriggerReq(const NrMacSchedSapProvider::SchedUlTriggerReqParameters& params)
{
     const auto& ueMap = GetUeMap(); // 使用父类提供的接口获取 m_ueMap
    // 更新所有UE的上行等待时间
    for (auto& ueEntry : ueMap)
    {
        auto ueInfo = std::dynamic_pointer_cast<NrMacSchedulerUeInfoDP>(ueEntry.second);
        if (ueInfo)
        {
            ueInfo->IncrementUlWaitTime();
            NS_LOG_DEBUG("UE " << ueInfo->m_rnti << " UL wait time: " << ueInfo->GetUlWaitTime());
        }
    }
    NrMacSchedulerOfdmaPF::DoSchedUlTriggerReq(params);
}

} // namespace ns3