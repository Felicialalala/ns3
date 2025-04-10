// Copyright (c) 2020 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * \ingroup examples
 * \file cttc-channel-randomness.cc
 *
 * This example is intended to test the randmness of the channel in order to see
 * if we can reproduce the same channel realization within the same simulation run.
 *
 * This example is needed for the RemHelper generation task in order to decide
 * how to handle the randomness and how to calculate different RemPoints without
 * having these calculations correlate.
 * 用于测试信道的随机性，以确定在同一仿真运行中是否能够重现相同的信道实现
 *
 * \code{.unparsed}
$ ./ns3 run "cttc-channel-randomness --PrintHelp"
    \endcode
 *
 */

#include "ns3/antenna-module.h"
#include "ns3/config-store-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/nr-spectrum-value-helper.h"
#include "ns3/simple-net-device.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/three-gpp-propagation-loss-model.h"
#include "ns3/three-gpp-spectrum-propagation-loss-model.h"
#include <ns3/spectrum-model.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CttcChannelRandomness");

/**波束成形函数
 * 这个函数用于执行基于离散傅里叶变换（DFT）的波束成形。
 * 它计算两个设备之间的方位角和仰角，然后根据天线元素的位置和这些角度计算天线权重，最后将权重设置到天线对象中
 * Perform the beamforming using the DFT beamforming method
 * \param thisDevice the device performing the beamforming
 * \param thisAntenna the antenna object associated to thisDevice
 * \param otherDevice the device towards which point the beam
 */
static void
DoBeamforming(Ptr<NetDevice> thisDevice,
              Ptr<UniformPlanarArray> thisAntenna,
              Ptr<NetDevice> otherDevice)
{
    // retrieve the position of the two devices
    Vector aPos = thisDevice->GetNode()->GetObject<MobilityModel>()->GetPosition();
    Vector bPos = otherDevice->GetNode()->GetObject<MobilityModel>()->GetPosition();

    // compute the azimuth and the elevation angles
    Angles completeAngle(bPos, aPos);

    double posX = bPos.x - aPos.x;
    double phiAngle = atan((bPos.y - aPos.y) / posX);

    if (posX < 0)
    {
        phiAngle = phiAngle + M_PI;
    }
    if (phiAngle < 0)
    {
        phiAngle = phiAngle + 2 * M_PI;
    }

    double hAngleRadian = fmod((phiAngle + M_PI), 2 * M_PI - M_PI); // the azimuth angle
    double vAngleRadian = completeAngle.GetInclination();           // the elevation angle

    // retrieve the number of antenna elements
    size_t totNoArrayElements = thisAntenna->GetNumElems();

    // the total power is divided equally among the antenna elements
    double power = 1 / sqrt(totNoArrayElements);

    UniformPlanarArray::ComplexVector antennaWeights(totNoArrayElements);
    // compute the antenna weights
    for (size_t ind = 0; ind < totNoArrayElements; ind++)
    {
        Vector loc = thisAntenna->GetElementLocation(ind);
        double phase = -2 * M_PI *
                       (sin(vAngleRadian) * cos(hAngleRadian) * loc.x +
                        sin(vAngleRadian) * sin(hAngleRadian) * loc.y + cos(vAngleRadian) * loc.z);
        antennaWeights[ind] = exp(std::complex<double>(0, phase)) * power;
    }

    // store the antenna weights
    thisAntenna->SetBeamformingVector(antennaWeights);
}

void PrintChannelMatrix(Ptr<const ThreeGppChannelModel::ChannelMatrix> matrix,
    const std::string& label,
    Ptr<const SpectrumModel> spectrumModel)
{
    uint32_t rows = matrix->m_channel.GetNumRows();
    uint32_t cols = matrix->m_channel.GetNumCols();
    uint32_t pages = matrix->m_channel.GetNumPages();

    std::cout << "\n====== Channel Matrix: " << label << " ======" << std::endl;
    for (uint32_t k = 0; k < pages; ++k)
    {
        double fc = std::next(spectrumModel->Begin(), k)->fc;
        std::cout << "RB[" << k << "] center frequency = " << fc / 1e6 << " MHz" << std::endl;
        for (uint32_t i = 0; i < rows; ++i)
        {
            for (uint32_t j = 0; j < cols; ++j)
            {
                std::complex<double> h = matrix->m_channel(i, j, k);
                std::cout << "H[" << i << "][" << j << "][" << k << "] = "
                    << h << " | abs=" << std::abs(h)
                    << ", phase=" << std::arg(h) << std::endl;
            }
        }
    }
}





int
main(int argc, char* argv[])
{
    double frequency = 28.0e9;
    uint32_t rbNum = 555; // bandwidth in number of RBs, for numerology 0 is equivalent to 555 RBs
    double subcarrierSpacing = 15000; // subcarrier spacing for numerology 0

    double txPower = 40;
    double distance = 10.0;
    std::string scenario = "UMa"; // 3GPP propagation scenario

    uint32_t simTimeMs = 1000;
    bool logging = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frequency",
                 "The operating frequency in Hz (2125.0e6 corresponds to EARFCN 2100)",
                 frequency);
    cmd.AddValue("rbNum", "The system BW in number of resource blocks", rbNum);
    cmd.AddValue("subcarrierSpacing", "The subcarrier spacing", subcarrierSpacing);
    cmd.AddValue("txPower", "The transmission power in dBm", txPower);
    cmd.AddValue("distance", "The distance between tx and rx nodes in meters", distance);
    cmd.AddValue("scenario",
                 "The 3GPP propagation scenario for the simulation."
                 "Choose among 'UMa'and 'UMi-StreetCanyon'",
                 scenario);
    cmd.AddValue("simTimeMs", "Simulation time in ms", simTimeMs);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod",
                       TimeValue(MilliSeconds(0))); // update the channel at each iteration
    Config::SetDefault("ns3::ThreeGppChannelConditionModel::UpdatePeriod",
                       TimeValue(MilliSeconds(0.0))); // do not update the channel condition


    // create the tx and rx nodes
    NodeContainer nodes;
    nodes.Create(2);

    // create the tx and rx devices
    Ptr<SimpleNetDevice> txDev = CreateObject<SimpleNetDevice>();
    Ptr<SimpleNetDevice> rxDev = CreateObject<SimpleNetDevice>();

    // associate the nodes and the devices
    nodes.Get(0)->AddDevice(txDev);
    txDev->SetNode(nodes.Get(0));
    nodes.Get(1)->AddDevice(rxDev);
    rxDev->SetNode(nodes.Get(1));

    // create the tx and rx mobility models, set the positions
    Ptr<MobilityModel> txMob = CreateObject<ConstantPositionMobilityModel>();
    txMob->SetPosition(Vector(0.0, 0.0, 10.0));
    Ptr<MobilityModel> rxMob = CreateObject<ConstantPositionMobilityModel>();
    rxMob->SetPosition(Vector(distance, 0.0, 1.6));

    // assign the mobility models to the nodes
    nodes.Get(0)->AggregateObject(txMob);
    nodes.Get(1)->AggregateObject(rxMob);

    //  设置随机数种子和运行编号
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    int64_t stream = 1;

    Ptr<ThreeGppPropagationLossModel> m_propagationLossModel; //!< the PropagationLossModel object
    Ptr<ThreeGppSpectrumPropagationLossModel>
        m_spectrumLossModel; //!< the SpectrumPropagationLossModel object

    // create and configure the factories for the channel condition and propagation loss models
    ObjectFactory propagationLossModelFactory;
    ObjectFactory channelConditionModelFactory;

    if (scenario == "UMa")
    {
        propagationLossModelFactory.SetTypeId(ThreeGppUmaPropagationLossModel::GetTypeId());
        channelConditionModelFactory.SetTypeId(AlwaysLosChannelConditionModel::GetTypeId());
    }
    else if (scenario == "UMi-StreetCanyon")
    {
        propagationLossModelFactory.SetTypeId(
            ThreeGppUmiStreetCanyonPropagationLossModel::GetTypeId());
        channelConditionModelFactory.SetTypeId(AlwaysLosChannelConditionModel::GetTypeId());
    }
    else
    {
        NS_FATAL_ERROR("The scenario can be 'UMa'or 'UMi-StreetCanyon'");
    }

    // create the propagation loss model 创建传播损耗模型并设置属性
    m_propagationLossModel = propagationLossModelFactory.Create<ThreeGppPropagationLossModel>();
    m_propagationLossModel->SetAttribute("Frequency", DoubleValue(frequency));
    m_propagationLossModel->SetAttribute("ShadowingEnabled", BooleanValue(false));

    // create the spectrum propagation loss model 创建频谱传播损耗模型并设置属性
    m_spectrumLossModel = CreateObject<ThreeGppSpectrumPropagationLossModel>();
    m_spectrumLossModel->SetChannelModelAttribute("Frequency", DoubleValue(frequency));
    m_spectrumLossModel->SetChannelModelAttribute("Scenario", StringValue(scenario));

    // create the channel condition model and associate it with the spectrum and
    // propagation loss model 创建信道条件模型并关联到频谱和传播损耗模型
    Ptr<ChannelConditionModel> condModel =
        channelConditionModelFactory.Create<ChannelConditionModel>();
    m_spectrumLossModel->SetChannelModelAttribute("ChannelConditionModel", PointerValue(condModel));
    m_propagationLossModel->SetChannelConditionModel(condModel);

    // create the channel model 创建信道模型并设置属性
    Ptr<ThreeGppChannelModel> channelModel = CreateObject<ThreeGppChannelModel>();
    channelModel->SetAttribute("Frequency", DoubleValue(frequency));
    channelModel->SetAttribute("Scenario", StringValue(scenario));
    channelModel->SetAttribute("ChannelConditionModel", PointerValue(condModel));


    // create the antenna objects and set their dimensions 创建天线对象并设置尺寸
    Ptr<UniformPlanarArray> txAntenna =
        CreateObjectWithAttributes<UniformPlanarArray>("NumColumns",
                                                       UintegerValue(2),
                                                       "NumRows",
                                                       UintegerValue(2));
    Ptr<UniformPlanarArray> rxAntenna =
        CreateObjectWithAttributes<UniformPlanarArray>("NumColumns",
                                                       UintegerValue(2),
                                                       "NumRows",
                                                       UintegerValue(2));

    // set the beamforming vectors
    DoBeamforming(txDev, txAntenna, rxDev);
    DoBeamforming(rxDev, rxAntenna, txDev);

    // 为信道模型分配流
    channelModel->AssignStreams(stream);

    // 获取第一个信道矩阵
    Ptr<const ThreeGppChannelModel::ChannelMatrix> channelMatrix1 =
        channelModel->GetChannel(txMob, rxMob, txAntenna, rxAntenna);



    // 创建频谱模型和发射功率谱密度  
    Ptr<const SpectrumModel> sm1 =
        NrSpectrumValueHelper::GetSpectrumModel(rbNum, frequency, subcarrierSpacing);
        
    std::cout << "Total number of RBs (bands) in SpectrumModel = " << sm1->GetNumBands() << std::endl;
    std::cout << "Channel matrix pages (RBs): " << channelMatrix1->m_channel.GetNumPages() << std::endl;


    std::vector<int> activeRbs;
    for (size_t rbId = 0; rbId < sm1->GetNumBands(); rbId++)
    {
        activeRbs.push_back(rbId);
    }
    Ptr<const SpectrumValue> txPsd1 = NrSpectrumValueHelper::CreateTxPowerSpectralDensity(
        txPower,
        activeRbs,
        sm1,
        NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_BW);
    Ptr<SpectrumSignalParameters> txParams1 = Create<SpectrumSignalParameters>();
    txParams1->psd = txPsd1->Copy();
    std::cout << "Average tx power 1: "
              << 10 *
                     log10(Sum(*txParams1->psd) / txParams1->psd->GetSpectrumModel()->GetNumBands())
              << " dBm" << std::endl;
    Ptr<SpectrumSignalParameters> rxParams1 =
        m_spectrumLossModel->DoCalcRxPowerSpectralDensity(txParams1,
                                                          txMob,
                                                          rxMob,
                                                          txAntenna,
                                                          rxAntenna);
    std::cout << "Average rx power 1: "
              << 10 * log10(Sum(*(rxParams1->psd)) /
                            rxParams1->psd->GetSpectrumModel()->GetNumBands())
              << " dBm" << std::endl;
    
    // 打印信道矩阵
    //PrintChannelMatrix(channelMatrix1,"channelMatrix1", sm1);

    // 创建新的信道模型并重复上述步骤          
    channelModel = CreateObject<ThreeGppChannelModel>();
    channelModel->SetAttribute("Frequency", DoubleValue(frequency));
    channelModel->SetAttribute("Scenario", StringValue(scenario));
    channelModel->SetAttribute("ChannelConditionModel", PointerValue(condModel));

    channelModel->AssignStreams(stream);

    Ptr<const ThreeGppChannelModel::ChannelMatrix> channelMatrix2 =
        channelModel->GetChannel(txMob, rxMob, txAntenna, rxAntenna);


    // 比较两个信道矩阵  
    if (channelMatrix1->m_channel != channelMatrix2->m_channel)
    {
        std::cout << "matrices are different" << std::endl;
    }
    else
    {
        // 通过固定随机数种子、运行编号和流编号，验证了在同一仿真运行中，相同配置下的信道实现可以被完全重现
        std::cout << "matrices are the same" << std::endl;
    }

    Ptr<const SpectrumModel> sm2 =
        NrSpectrumValueHelper::GetSpectrumModel(rbNum, frequency, subcarrierSpacing);
    std::vector<int> activeRbs2;
    for (size_t rbId = 0; rbId < sm2->GetNumBands(); rbId++)
    {
        activeRbs2.push_back(rbId);
    }
    Ptr<const SpectrumValue> txPsd2 = NrSpectrumValueHelper::CreateTxPowerSpectralDensity(
        txPower,
        activeRbs2,
        sm2,
        NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_BW);
    Ptr<SpectrumSignalParameters> txParams2 = Create<SpectrumSignalParameters>();
    txParams2->psd = txPsd2->Copy();

    std::cout << "Average tx power 1: "
              << 10 *
                     log10(Sum(*txParams2->psd) / txParams2->psd->GetSpectrumModel()->GetNumBands())
              << " dBm" << std::endl;
    Ptr<SpectrumSignalParameters> rxParams2 =
        m_spectrumLossModel->DoCalcRxPowerSpectralDensity(txParams2,
                                                          txMob,
                                                          rxMob,
                                                          txAntenna,
                                                          rxAntenna);
    std::cout << "Average rx power 1: "
              << 10 * log10(Sum(*(rxParams2->psd)) /
                            rxParams2->psd->GetSpectrumModel()->GetNumBands())
              << " dBm" << std::endl;

    Simulator::Stop(MilliSeconds(simTimeMs));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}
