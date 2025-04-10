// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-spectrum-value-helper.h"

#include "ns3/simulator.h"
#include <ns3/abort.h>
#include <ns3/fatal-error.h>
#include <ns3/log.h>

#include <cmath>
#include <map>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrSpectrumValueHelper");

// 定义结构体 NrSpectrumModelId 来存储频谱模型的相关信息
struct NrSpectrumModelId
{
    /**
     * Constructor
     * 结构体的构造函数，接收中心频率、带宽（以RB为单位）和子载波间隔作为参数
     * \param f center frequency
     * \param b bandwidth in RBs
     * \param s subcarrierSpacing
     */
    NrSpectrumModelId(double f, uint16_t b, double s);
    double frequency;   ///<
    uint16_t bandwidth; ///< bandwidth 
    double subcarrierSpacing;
};

NrSpectrumModelId::NrSpectrumModelId(double f, uint16_t b, double s)
    : frequency(f),
      bandwidth(b),
      subcarrierSpacing(s)
{
}

/**
 * 重载 < 运算符，以便该结构体可以作为 g_nrSpectrumModelMap 的键
 * 比较规则：先比较中心频率，若相等则比较带宽，若仍相等则比较子载波间隔
 * \param a 左侧对象 lhs
 * \param b 右侧对象 rhs
 * \returns 如果中心频率小于，或中心频率相等但带宽小于，或中心频率和带宽相等但子载波间隔小于，则返回 true
 * \brief Operator < so that it can be the key in a g_nrSpectrumModelMap
 * \returns true if earfcn less than or if earfcn equal and bandwidth less than or if earfcn and
 * bandwidth equal and sucarrier spacing less than
 */
bool
operator<(const NrSpectrumModelId& a, const NrSpectrumModelId& b)
{
    return ((a.frequency < b.frequency) ||
            ((a.frequency == b.frequency) && (a.bandwidth < b.bandwidth)) ||
            ((a.frequency == b.frequency) && (a.bandwidth == b.bandwidth) &&
             (a.subcarrierSpacing < b.subcarrierSpacing)));
}

// 静态映射存储频谱模型，键为 NrSpectrumModelId，值为 SpectrumModel 的指针
static std::map<NrSpectrumModelId, Ptr<SpectrumModel>>
    g_nrSpectrumModelMap; ///< nr spectrum model map

// 获取频谱模型的函数
Ptr<const SpectrumModel>
NrSpectrumValueHelper::GetSpectrumModel(uint32_t numRbs,
                                        double centerFrequency,
                                        double subcarrierSpacing)
{
    NS_LOG_FUNCTION(centerFrequency << numRbs << subcarrierSpacing);

    // 进行一些参数检查，如带宽不能为 0，中心频率范围应在 0.5GHz 到 100GHz 之间，子载波间隔只能取几个指定的值
    NS_ABORT_MSG_IF(numRbs == 0, "Total bandwidth cannot be 0 RBs");
    NS_ABORT_MSG_IF(centerFrequency < 0.5e9 || centerFrequency > 100e9,
                    "Central frequency should be in range from 0.5GHz to 100GHz");
    NS_ABORT_MSG_IF(subcarrierSpacing != 15000 && subcarrierSpacing != 30000 &&
                        subcarrierSpacing != 60000 && subcarrierSpacing != 120000 &&
                        subcarrierSpacing != 240000 && subcarrierSpacing != 480000,
                    "Supported subcarrier spacing values are: 15000, 30000, 60000, 120000, 240000 "
                    "and 480000 Hz.");

    // 创建 NrSpectrumModelId 对象
    NrSpectrumModelId modelId = NrSpectrumModelId(centerFrequency, numRbs, subcarrierSpacing);
    if (g_nrSpectrumModelMap.find(modelId) == g_nrSpectrumModelMap.end())
    {
        NS_ASSERT_MSG(centerFrequency != 0, "The carrier frequency cannot be set to 0");
        // 计算起始频率
        double f = centerFrequency - (numRbs * subcarrierSpacing * SUBCARRIERS_PER_RB / 2.0);
        Bands rbs; // 存储rb资源块的向量 A vector representing all resource blocks
        for (uint32_t numrb = 0; numrb < numRbs; ++numrb)
        {
            BandInfo rb;
            // 计算资源块的频率范围
            rb.fl = f;
            f += subcarrierSpacing * SUBCARRIERS_PER_RB / 2;
            rb.fc = f;
            f += subcarrierSpacing * SUBCARRIERS_PER_RB / 2;
            rb.fh = f;
            rbs.push_back(rb);
        }

        // 创建频谱模型
        Ptr<SpectrumModel> model = Create<SpectrumModel>(rbs);
        // 将创建的频谱模型保存到映射中save this model to the map of spectrum models
        g_nrSpectrumModelMap.insert(
            std::pair<NrSpectrumModelId, Ptr<SpectrumModel>>(modelId, model));
        // 调度删除频谱值的操作
        Simulator::ScheduleDestroy(&NrSpectrumValueHelper::DeleteSpectrumValues);
        NS_LOG_INFO("Created SpectrumModel with frequency: "
                    << f << " NumRB: " << rbs.size() << " subcarrier spacing: " << subcarrierSpacing
                    << ", and global UID: " << model->GetUid());
    }
    // 返回存储在映射中的频谱模型
    return g_nrSpectrumModelMap.find(modelId)->second;
}

// 创建传输功率谱密度，只在激活的资源块上分配功率
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateTxPsdOverActiveRbs(double powerTx,
                                                const std::vector<int>& activeRbs,
                                                const Ptr<const SpectrumModel>& spectrumModel)
{
    NS_LOG_FUNCTION(powerTx << activeRbs << spectrumModel);
    // 创建频谱值對象
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(spectrumModel);
    // 将功率从 dBm 转换为 W
    double powerTxW = std::pow(10., (powerTx - 30) / 10);
    double txPowerDensity = 0;
    // 计算子带宽度
    double subbandWidth = (spectrumModel->Begin()->fh - spectrumModel->Begin()->fl);
    // 进行参数检查，RB 宽度应大于或等于 180KHz
    NS_ABORT_MSG_IF(subbandWidth < 180000,
                    "Erroneous spectrum model. RB width should be equal or greater than 180KHz");
    // 计算功率密度
    txPowerDensity = powerTxW / (subbandWidth * activeRbs.size());
    // 在激活的资源块上设置功率密度
    for (int rbId : activeRbs)
    {
        (*txPsd)[rbId] = txPowerDensity;
    }
    NS_LOG_LOGIC(*txPsd);
    return txPsd;
}

// 创建传输功率谱密度，在所有资源块上分配功率
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateTxPsdOverAllRbs(double powerTx,
                                             const std::vector<int>& activeRbs,
                                             const Ptr<const SpectrumModel>& spectrumModel)
{
    NS_LOG_FUNCTION(powerTx << activeRbs << spectrumModel);
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(spectrumModel);
    double powerTxW = std::pow(10., (powerTx - 30) / 10);
    double txPowerDensity = 0;
    double subbandWidth = (spectrumModel->Begin()->fh - spectrumModel->Begin()->fl);
    NS_ABORT_MSG_IF(subbandWidth < 180000,
                    "Erroneous spectrum model. RB width should be equal or greater than 180KHz");
    // 计算功率密度，基于所有资源块的数量
    txPowerDensity = powerTxW / (subbandWidth * spectrumModel->GetNumBands());
    for (int rbId : activeRbs)
    {
        (*txPsd)[rbId] = txPowerDensity;
    }
    NS_LOG_LOGIC(*txPsd);
    return txPsd;
}

// 新增，根据用户自定义的每个资源块（RB）上的功率值（以 dBm 为单位），创建一个表示发射功率谱密度（PSD）的 SpectrumValue 对象。
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateTxPsdPerRbPower(
    const std::vector<double>& powerPerRbDbm,
    const Ptr<const SpectrumModel>& spectrumModel)
{
    NS_LOG_FUNCTION(powerPerRbDbm << spectrumModel);
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(spectrumModel);

    for (size_t rbId = 0; rbId < powerPerRbDbm.size(); ++rbId)
    {  
        if (rbId >= spectrumModel->GetNumBands())
        {
            NS_LOG_WARN("powerPerRbDbm size > RB count; ignoring extra values");
            break;
        }
        double powerW = std::pow(10., (powerPerRbDbm[rbId] - 30) / 10); // dBm → W
        auto bandIt = spectrumModel->Begin() + rbId;
		double subbandWidth = bandIt->fh - bandIt->fl;
        //将计算得到的功率谱密度（功率除以带宽）设置到 txPsd 中对应的 RB 位置上
        (*txPsd)[rbId] = powerW / subbandWidth;
    }

    NS_LOG_LOGIC(*txPsd);
    return txPsd;
}



// 根据不同的功率分配类型创建传输功率谱密度
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateTxPowerSpectralDensity(double powerTx,
                                                    const std::vector<int>& rbIndexVector,
                                                    const Ptr<const SpectrumModel>& txSm,
                                                    enum PowerAllocationType allocationType)
{
    switch (allocationType)
    {
    case UNIFORM_POWER_ALLOCATION_BW: {
        // 在所有资源块上均匀分配功率
        return CreateTxPsdOverAllRbs(powerTx, rbIndexVector, txSm);
    }
    case UNIFORM_POWER_ALLOCATION_USED: {
        // 仅在激活的资源块上均匀分配功率
        return CreateTxPsdOverActiveRbs(powerTx, rbIndexVector, txSm);
    }
    case CUSTOM_PER_RB_POWER_ALLOCATION: {
        NS_FATAL_ERROR("CUSTOM_PER_RB_POWER_ALLOCATION must use the vector<double> interface for per-RB dBm input.");
    }
    default: {
        // 未知功率分配类型时触发致命错误
        NS_FATAL_ERROR("Unknown power allocation type.");
    }
    }
}

//添加一个新的重载接口来处理 per-RB 功率输入
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateTxPowerSpectralDensity(
    const std::vector<double>& powerPerRbDbm,
    const Ptr<const SpectrumModel>& spectrumModel,
    enum PowerAllocationType allocationType)
{
    switch (allocationType)
    {
    case CUSTOM_PER_RB_POWER_ALLOCATION: {
        return CreateTxPsdPerRbPower(powerPerRbDbm, spectrumModel);
    }
    default:
        NS_FATAL_ERROR("This overload of CreateTxPowerSpectralDensity only supports CUSTOM_PER_RB_POWER_ALLOCATION.");
    }
}

// 创建噪声功率谱密度
Ptr<SpectrumValue>
NrSpectrumValueHelper::CreateNoisePowerSpectralDensity(
    double noiseFigureDb,
    const Ptr<const SpectrumModel>& spectrumModel)
{
    NS_LOG_FUNCTION(noiseFigureDb << spectrumModel);
    // 热噪声的参考值（-174.0 dBm/Hz）
    const double kT_dBm_Hz = -174.0; // dBm/Hz
    // 将热噪声从 dBm/Hz 转换为 W/Hz
    double kT_W_Hz = std::pow(10.0, (kT_dBm_Hz - 30) / 10.0);
    // 将噪声系数从 dB 转换为线性值
    double noiseFigureLinear = std::pow(10.0, noiseFigureDb / 10.0);
    // 计算噪声功率谱密度
    double noisePowerSpectralDensity = kT_W_Hz * noiseFigureLinear;

    Ptr<SpectrumValue> noisePsd = Create<SpectrumValue>(spectrumModel);
    (*noisePsd) = noisePowerSpectralDensity;
    return noisePsd;
}

// 获取有效带宽
uint64_t
NrSpectrumValueHelper::GetEffectiveBandwidth(double bandwidth, uint8_t numerology)
{
    NS_LOG_FUNCTION(bandwidth << numerology);
    // 计算子载波间隔
    uint32_t scSpacing = 15000 * static_cast<uint32_t>(std::pow(2, numerology));
    // 计算资源块数量
    uint32_t numRbs = static_cast<uint32_t>(bandwidth / (scSpacing * SUBCARRIERS_PER_RB));
    NS_LOG_DEBUG("Total bandwidth: " << bandwidth << " effective bandwidth:"
                                     << numRbs * (scSpacing * SUBCARRIERS_PER_RB));
    return numRbs * (scSpacing * SUBCARRIERS_PER_RB);
}

// 清除频谱模型映射
void
NrSpectrumValueHelper::DeleteSpectrumValues()
{
    g_nrSpectrumModelMap.clear();
}

} // namespace ns3
