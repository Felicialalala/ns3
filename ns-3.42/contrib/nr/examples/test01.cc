#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ideal-beamforming-algorithm.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-mac-scheduler-tdma-rr.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/nr-eesm-error-model.h" 
//#include "ns3/propagation-loss-model.h"
#include "ns3/three-gpp-spectrum-propagation-loss-model.h"
#include "ns3/matrix-based-channel-model.h"
#include "ns3/phased-array-model.h"
#include <vector>
#include <algorithm>
#include <random>
#include <map>
#include <utility> // for std::pair
#include <fstream> 
#include <iomanip> 
#include "ns3/recentbler.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("test02");

std::map<uint16_t, std::map<uint16_t, RecentBler>> recentBlerMap;

// 存储每个UE的MCS值
struct McsStats
{
    std::vector<uint8_t> mcsValues; // 存储MCS值的向量
};

// 存储每个UE的Tbler值
struct TblerStats
{
    std::vector<uint8_t> TblerValues; 
};


// 全局MCS统计结构 (Key: <CellId, RNTI>)
using McsKey = std::pair<uint16_t, uint16_t>; // first: CellId, second: RNTI
std::map<McsKey, McsStats> g_dlMcsStats;    // 下行MCS统计
std::map<McsKey, McsStats> g_ulMcsStats;    // 上行MCS统计

// 全局tbler统计结构 (Key: <CellId, RNTI>)
using TblerKey = std::pair<uint16_t, uint16_t>; // first: CellId, second: RNTI
std::map<TblerKey, TblerStats> g_dlTblerStats;    
std::map<TblerKey, TblerStats> g_ulTblerStats;    

// 全局 BLER 统计结构
struct BlerStats
{
    uint32_t totalTb = 0;     // 总传输块数
    uint32_t corruptedTb = 0; // 错误传输块数
    uint64_t totalMcs = 0;    // 新增：累计MCS值总和
    uint64_t totalTbler = 0;
    uint64_t totalRb = 0;
};

// 全局统计结构 (Key: <CellId, RNTI>)
using BlerKey = std::pair<uint16_t, uint16_t>; // first: CellId, second: RNTI
std::map<BlerKey, BlerStats> g_dlBlerStats;    // 下行统计
std::map<BlerKey, BlerStats> g_ulBlerStats;    // 上行统计

double halfTime;

// 以 cell id 为单位的全局统计结构 (Key: CellId)
std::map<uint16_t, BlerStats> g_dlBlerStatsByCell; // 下行统计
std::map<uint16_t, BlerStats> g_ulBlerStatsByCell; // 上行统计

// 新增结构存储最近50次传输块状态
struct RecentBlerStats
{
    std::deque<bool> recentCorrupted; // 存储最近50次传输块状态
};

// 全局最近BLER统计结构
std::map<BlerKey, RecentBlerStats> g_dlRecentBlerStats; // 下行
std::map<BlerKey, RecentBlerStats> g_ulRecentBlerStats; // 上行

// 全局变量记录最大RB数
uint32_t maxDlRb = 0;
uint32_t maxUlRb = 0;

void NotifyRxDataTraceUe(ns3::RxPacketTraceParams params)
{
    if (params.m_rbAssignedNum > maxDlRb) {
        maxDlRb = params.m_rbAssignedNum;
    }
    BlerKey key(params.m_cellId, params.m_rnti);

        

    // 记录MCS
    g_dlBlerStats[key].totalMcs += params.m_mcs;
    g_dlBlerStats[key].totalTbler += params.m_tbler;
    g_dlBlerStats[key].totalRb += params.m_rbAssignedNum;
    g_dlMcsStats[key].mcsValues.push_back(params.m_mcs); // 存储MCS值
    g_dlTblerStats[key].TblerValues.push_back(params.m_tbler);

        
    if (params.m_corrupt) {
        g_dlBlerStats[key].corruptedTb++;
        g_dlBlerStatsByCell[params.m_cellId].corruptedTb++;
    }
    g_dlBlerStats[key].totalTb++;
    g_dlBlerStatsByCell[params.m_cellId].totalTb++;
    g_dlBlerStatsByCell[params.m_cellId].totalMcs += params.m_mcs;
    g_dlBlerStatsByCell[params.m_cellId].totalRb += params.m_rbAssignedNum;
    NS_LOG_UNCOND("rbBitmap:"<< params.m_rbBitmap.size());

    // 新增：更新最近50次下行统计
    auto& recentStats = g_dlRecentBlerStats[key];
    recentStats.recentCorrupted.push_back(params.m_corrupt);
    if (recentStats.recentCorrupted.size() > 50) {
        recentStats.recentCorrupted.pop_front();
    }

    // 更新 recentBlerMap
    auto& recentBlerEntry = recentBlerMap[params.m_cellId][params.m_rnti];
    recentBlerEntry.AddSample(params.m_corrupt);
}

void NotifyRxDataTraceGnb(ns3::RxPacketTraceParams params)
{
    if (params.m_rbAssignedNum > maxUlRb) {
        maxUlRb = params.m_rbAssignedNum;
    }

    BlerKey key(params.m_cellId, params.m_rnti);

    // 记录MCS
    g_ulBlerStats[key].totalMcs += params.m_mcs; 
    g_ulBlerStats[key].totalTbler += params.m_tbler;
    g_ulBlerStats[key].totalRb += params.m_rbAssignedNum;
    g_ulMcsStats[key].mcsValues.push_back(params.m_mcs); // 存储MCS值
    
    if (params.m_corrupt) {
        g_ulBlerStats[key].corruptedTb++;
        g_ulBlerStatsByCell[params.m_cellId].corruptedTb++;
    }
    g_ulBlerStats[key].totalTb++;
    g_ulBlerStatsByCell[params.m_cellId].totalTb++;
    g_ulBlerStatsByCell[params.m_cellId].totalMcs += params.m_mcs;
    g_ulBlerStatsByCell[params.m_cellId].totalRb += params.m_rbAssignedNum;
    //NS_LOG_UNCOND("rbBitmap:"<< params.m_rbBitmap.size());
    //NS_LOG_UNCOND("rbAssigned:"<< params.m_rbAssignedNum);
    // 新增：更新最近50次上行统计
    auto& recentStats = g_ulRecentBlerStats[key];
    recentStats.recentCorrupted.push_back(params.m_corrupt);
    if (recentStats.recentCorrupted.size() > 50) {
        recentStats.recentCorrupted.pop_front();
    }

    // 更新 recentBlerMap
    auto& recentBlerEntry = recentBlerMap[params.m_cellId][params.m_rnti];
    recentBlerEntry.AddSample(params.m_corrupt);
}

void WriteRecentBlerStats(const std::string& filename)
{
    std::ofstream recentFile(filename);

    recentFile << "===== Recent BLER (Last 50 TBs) =====" << std::endl;
    for (const auto& cellEntry : recentBlerMap) {
        uint16_t cellId = cellEntry.first;
        for (const auto& ueEntry : cellEntry.second) {
            uint16_t rnti = ueEntry.first;
            const RecentBler& stats = ueEntry.second;
            
            recentFile << "Cell " << cellId 
                      << " RNTI " << rnti
                      << ": RecentBLER=" 
                      << std::fixed << std::setprecision(2)
                      << stats.GetBler() * 100 << "%"
                      << " (Based on " << stats.recentCorrupted.size() << " samples)"
                      << std::endl;
        }
    }
    
    recentFile << "===== DL Recent BLER (Last 50 TBs) =====" << std::endl;
    for (const auto& entry : g_dlRecentBlerStats) {
        const auto& stats = entry.second;
        size_t total = stats.recentCorrupted.size();
        size_t corrupted = std::count(stats.recentCorrupted.begin(), stats.recentCorrupted.end(), true);
        
        recentFile << "Cell " << entry.first.first 
                  << " RNTI " << entry.first.second
                  << ": RecentBLER=" 
                  << std::fixed << std::setprecision(2)
                  << (total ? (corrupted * 100.0 / total) : 0.0) << "%"
                  << " (Based on " << total << " samples)"
                  << std::endl;
    }

    recentFile << "\n===== UL Recent BLER (Last 50 TBs) =====" << std::endl;
    for (const auto& entry : g_ulRecentBlerStats) {
        const auto& stats = entry.second;
        size_t total = stats.recentCorrupted.size();
        size_t corrupted = std::count(stats.recentCorrupted.begin(), stats.recentCorrupted.end(), true);
        
        recentFile << "Cell " << entry.first.first 
                  << " RNTI " << entry.first.second
                  << ": RecentBLER=" 
                  << std::fixed << std::setprecision(2)
                  << (total ? (corrupted * 100.0 / total) : 0.0) << "%"
                  << " (Based on " << total << " samples)"
                  << std::endl;
    }

    recentFile.close();
}


// 修改基站和用户位置配置逻辑
void ConfigurePositions(NodeContainer& gNbNodes, NodeContainer& ueNodes, MobilityHelper& mobility, uint16_t gNbNum, uint16_t ueNum)
{
    double gNbHeight = 10; // 基站高度
    double ueHeight = 1.5;
    double cellRadius = 1000; // 单基站覆盖半径或蜂窝边长
    gNbNodes.Create(gNbNum);
    ueNodes.Create(ueNum);
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    Ptr<RandomDiscPositionAllocator> uePositionAlloc = CreateObject<RandomDiscPositionAllocator>();
    // 获取当前时间作为种子
    std::random_device rd;
    unsigned seed = rd();
    //unsigned seed = static_cast<unsigned>(std::time(nullptr));
    std::mt19937 generator(seed); // 使用 Mersenne Twister 算法作为随机数生成器

    // 单基站情况
    if (gNbNum == 1)
    {
        // 基站位置
        gnbPositionAlloc->Add(Vector(0.0, 0.0, gNbHeight));
        mobility.SetPositionAllocator(gnbPositionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gNbNodes);
        
        // 用户随机分布在基站周围圆形区域内
        uePositionAlloc->SetAttribute("X", DoubleValue(0.0));
        uePositionAlloc->SetAttribute("Y", DoubleValue(0.0));
        Ptr<UniformRandomVariable> rho = CreateObject<UniformRandomVariable>();
        rho->SetAttribute("Min", DoubleValue(0.0));
        rho->SetAttribute("Max", DoubleValue(cellRadius));
        uePositionAlloc->SetAttribute("Rho", PointerValue(rho)); // 使用自定义的随机变量
        //uePositionAlloc->SetAttribute("Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string(cellRadius) + "]"));
        mobility.SetPositionAllocator(uePositionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(ueNodes);
    }
    // 7个基站的蜂窝布局
    else 
    {
        // 计算周围6个基站的坐标
        std::vector<Vector> gnbPositions;
        gnbPositions.push_back(Vector(0.0, 0.0, gNbHeight)); // 中心基站
        for (int i = 1; i < gNbNum; ++i)
        {
            double angle = i * 60.0 * M_PI / 180.0; // 60度转换为弧度
            double x = cellRadius * cos(angle);
            double y = cellRadius * sin(angle);
            // 四舍五入到能被10整除的数
            x = std::round(x / 10) * 10;
            y = std::round(y / 10) * 10;
            gnbPositions.push_back(Vector(x, y, gNbHeight));
        }
        // 设置基站位置
        for (const auto& pos : gnbPositions)
        {
            gnbPositionAlloc->Add(pos);
        }
        mobility.SetPositionAllocator(gnbPositionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gNbNodes);

        // 用户随机分布在7个蜂窝网络覆盖范围内
        std::vector<Vector> cellCenters = gnbPositions;
        //std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, cellCenters.size() - 1);
        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            int cellIndex = distribution(generator);
            double x = cellCenters[cellIndex].x;
            double y = cellCenters[cellIndex].y;
            // 生成随机角度和半径
            std::uniform_real_distribution<double> angleDist(0, 2 * M_PI);
            std::uniform_real_distribution<double> radiusDist(0, cellRadius/3);
            double randomAngle = angleDist(generator);
            double randomRadius = radiusDist(generator);
            // 计算用户位置
            double ueX = x + randomRadius * cos(randomAngle);
            double ueY = y + randomRadius * sin(randomAngle);
            // 四舍五入到能被10整除的数
            ueX = std::round(ueX / 10) * 10;
            ueY = std::round(ueY / 10) * 10;
        
            Ptr<ListPositionAllocator> uePosAlloc = CreateObject<ListPositionAllocator>();
            uePosAlloc->Add(Vector(ueX, ueY, ueHeight));
            mobility.SetPositionAllocator(uePosAlloc);

            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(ueNodes.Get(i));
        }
    }

    // 打印基站和用户的位置
    for (uint32_t i = 0; i < gNbNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobility = gNbNodes.Get(i)->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        NS_LOG_UNCOND("gNB " << i << "位置: (" << pos.x << ", " << pos.y << ")");
    }

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobility = ueNodes.Get(i)->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        NS_LOG_UNCOND("UE " << i << "位置: (" << pos.x << ", " << pos.y << ")");
    }
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
    ns3::RngSeedManager::SetSeed(std::chrono::system_clock::now().time_since_epoch().count());

    uint16_t gNbNum = 1;
    uint16_t ueNum = 3;
    //uint16_t ueNumPergNb = 5;
    uint16_t numFlowsUe = 1;
    
    bool useRandomDistribution = false; // 新增参数，用于选择用户分布模式
    bool useWaterFilling = true; // 新增参数，用于选择功率分配方式
    double gnbPower = 20; // 单位为W


    uint8_t numBands = 1;
    // double centralFrequencyBand = 28e9;
    // double bandwidthBand = 3e9;
    double centralFrequencyBand = 2.4e9;
    double bandwidthBand = 100e6;

    uint16_t numerology = 1; // numerology for contiguous case, scs=30kHz

    
    std::string pattern =
        "F|F|F|F|F|F|F|F|F|F|"; // Pattern can be e.g. "DL|S|UL|UL|DL|DL|S|UL|UL|DL|"
    bool cellScan = false;
    double beamSearchAngleStep = 10.0;

    // 全缓冲模式，决定流量生成的行为（基于时间间隔 vs. 尽可能快地发送）。
    bool udpFullBuffer = false;
    // 流量包大小字节（Bytes）
    //uint32_t udpPacketSizeUll = 100;
    uint32_t udpPacketSizeBe = 1252;
    uint32_t udpPacketSizeDl = 500;
    uint32_t udpPacketSizeUl = 500;
    uint32_t lambdaUll = 10000;
    uint32_t lambdaBe = 1000;
    uint32_t lambdaDl= 200;
    uint32_t lambdaUl = 200;

    bool logging = false;

    bool disableDl = false;
    bool disableUl = true;
    double dlOnly = 1; //1:只有下行；0：只有上行
    double allocate = 1; // 0: BW; 1: USED
    double scheduler = 0; // 0: RR; 1: PF; 2: DP

    std::string simTag = "default";
    std::string outputDir = "./output/test01";

    double simTime = 5;           // seconds
    double udpAppStartTime = 0.4; // seconds

    double totalTxPower = 50; 

    CommandLine cmd(__FILE__);

    cmd.AddValue("useRandomDistribution", "Use random distribution mode for UE placement", useRandomDistribution);
    cmd.AddValue("useWaterFilling", "Use water-filling power allocation", useWaterFilling);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology", gNbNum);
    cmd.AddValue("ueNum", "The number of UE per gNb in multiple-ue topology", ueNum);
    cmd.AddValue("numBands",
                 "Number of operation bands. More than one implies non-contiguous CC",
                 numBands);
    cmd.AddValue("centralFrequencyBand",
                 "The system frequency to be used in band 1",
                 centralFrequencyBand);
    cmd.AddValue("bandwidthBand", "The system bandwidth to be used in band 1", bandwidthBand);
    cmd.AddValue("numerology", "Numerlogy to be used in contiguous case", numerology);
    cmd.AddValue("tddPattern",
                 "LTE TDD pattern to use (e.g. --tddPattern=DL|S|UL|UL|UL|DL|S|UL|UL|UL|)",
                 pattern);
    cmd.AddValue("totalTxPower",
                 "total tx power that will be proportionally assigned to"
                 " bandwidth parts depending on each BWP bandwidth ",
                 totalTxPower);
    cmd.AddValue("gnbPower",
                 "total tx power that will be proportionally assigned to"
                 " bandwidth parts depending on each BWP bandwidth (W) ",
                 gnbPower);
                 
    cmd.AddValue("cellScan",
                 "Use beam search method to determine beamforming vector,"
                 "true to use cell scanning method",
                 cellScan);
    cmd.AddValue("beamSearchAngleStep",
                 "Beam search angle step for beam search method",
                 beamSearchAngleStep);
    cmd.AddValue("udpFullBuffer",
                 "Whether to set the full buffer traffic; if this parameter is "
                 "set then the udpInterval parameter will be neglected.",
                 udpFullBuffer);
    cmd.AddValue("udpPacketSizeUl",
                 "packet size in bytes to be used by uplink traffic",
                 udpPacketSizeUl);
    cmd.AddValue("packetSizeBe",
                 "packet size in bytes to be used by best effort traffic",
                 udpPacketSizeBe);
    cmd.AddValue("lambdaUll",
                 "Number of UDP packets in one second for ultra low latency traffic",
                 lambdaUll);
    cmd.AddValue("lambdaBe",
                 "Number of UDP packets in one second for best effort traffic",
                 lambdaBe);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("disableDl", "Disable DL flow", disableDl);
    cmd.AddValue("disableUl", "Disable UL flow", disableUl);
    cmd.AddValue("simTag",
                 "tag to be appended to output filenames to distinguish simulation campaigns",
                 simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);
    cmd.AddValue("dlOnly",
                "只有下行或只有上行",
                dlOnly);
    cmd.AddValue("allocate",
                "激活rb分配或全部rb分配",
                allocate);
    cmd.AddValue("scheduler",
                "调度器类型",
                scheduler);

    cmd.Parse(argc, argv);

    NS_ABORT_IF(numBands < 1);
    NS_ABORT_MSG_IF(disableDl == true && disableUl == true, "Enable one of the flows");

    totalTxPower = 10 * std::log10(gnbPower * 1000);

    // 计算仿真后半段的起始时间
    halfTime = simTime / 2;

    if(dlOnly == 1){
        disableDl = false;
        disableUl = true;
    }else if (dlOnly == 0)
    {
        disableDl = true;
        disableUl = false;
    }else{
        disableDl = false;
        disableUl = false;
    }

    if(gNbNum == 7){
        ueNum = ueNum * 3;
    }
    

    // 打印变量的值以验证命令行参数是否正确更新
    NS_LOG_UNCOND("gNbNum: " << gNbNum);
    NS_LOG_UNCOND("ueNum: " << ueNum);
    NS_LOG_UNCOND("simTime: " << simTime);
    NS_LOG_UNCOND("totalTxPower: " << totalTxPower);

    // ConfigStore inputConfig;
    // inputConfig.ConfigureDefaults ();

    // enable logging or not
    if (logging)
    {
        //LogComponentEnable("Nr3gppPropagationLossModel", LOG_LEVEL_ALL);
        //LogComponentEnable("Nr3gppBuildingsPropagationLossModel", LOG_LEVEL_ALL);
        //LogComponentEnable("Nr3gppChannel", LOG_LEVEL_ALL);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_ALL);
        LogComponentEnable("NrPhyRxTrace",LOG_LEVEL_ALL);
        LogComponentEnable("NrMacScheduler",LOG_LEVEL_ALL);
        LogComponentEnable("NrUeNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("NrGnbNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("NrRlcUm", LOG_LEVEL_ALL);
        LogComponentEnable("NrGnbMac", LOG_LEVEL_ALL);
        LogComponentEnable("NrMacSchedulerCQIManagement", LOG_LEVEL_ALL);
        

    }

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));

    // create base stations and mobile terminals
    NodeContainer gNbNodes;
    NodeContainer ueNodes;
    MobilityHelper mobility;

    
    // 配置基站和用户位置
    ConfigurePositions(gNbNodes, ueNodes, mobility, gNbNum, ueNum);

    

    // setup the nr simulation
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // 添加以下配置，适配多核
    // nrHelper->SetGnbPhyAttribute("NumThreads", UintegerValue(32)); // 与核心数一致
    // nrHelper->SetUePhyAttribute("NumThreads", UintegerValue(32));

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);


    /*
     * Setup the configuration of the spectrum. There is a contiguous and a non-contiguous
     * example:
     * 1) One operation band is deployed with 4 contiguous component carriers
     *    (CC)s, which are automatically generated by the ccBwpManager
     * 2) One operation bands non-contiguous case. CCs and BWPs are manually created
     */

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;

    OperationBandInfo band;

    // For the case of manual configuration of CCs and BWPs
    // std::unique_ptr<ComponentCarrierInfo> cc0(new ComponentCarrierInfo());
    // std::unique_ptr<BandwidthPartInfo> bwp0(new BandwidthPartInfo());
    // std::unique_ptr<BandwidthPartInfo> bwp1(new BandwidthPartInfo());

    // std::unique_ptr<ComponentCarrierInfo> cc1(new ComponentCarrierInfo());
    // std::unique_ptr<BandwidthPartInfo> bwp2(new BandwidthPartInfo());

    /*
    * CC band configuration n257F (NR Release 15): four contiguous CCs of
    * 400MHz at maximum. In this automated example, each CC contains a single
    * BWP occupying the whole CC bandwidth.
    *
    * The configured spectrum division is:
    * ----------------------------- Band --------------------------------
    * ------CC0------|------CC1-------|-------CC2-------|-------CC3-------
    * ------BWP0-----|------BWP0------|-------BWP0------|-------BWP0------
    */

    const uint8_t numContiguousCcs = 1; // 4 CCs per Band

    // Create the configuration for the CcBwpHelper
    // 信道场景可选择：
    // RMa,RMa_LoS,RMa_nLoS,
    // UMa,UMa_LoS,UMa_nLoS,
    // UMi_StreetCanyon,UMi_StreetCanyon_LoS,UMi_StreetCanyon_nLoS,
    // InH_OfficeOpen,InH_OfficeOpen_LoS,InH_OfficeOpen_nLoS,
    // InH_OfficeMixed,InH_OfficeMixed_LoS,InH_OfficeMixed_nLoS,
    // UMa_Buildings,UMi_Buildings,
    // V2V_Highway,V2V_Urban
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequencyBand,
                                                    bandwidthBand,
                                                    numContiguousCcs,
                                                    BandwidthPartInfo::UMa_LoS);

    bandConf.m_numBwp = 1; // 1 BWP per CC

    // By using the configuration created, it is time to make the operation band
    band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));//信道模型设置
    // nrHelper->SetPhasedArraySpectrumPropagationLossModelTypeId(
    //         DistanceBasedThreeGppSpectrumPropagationLossModel::GetTypeId());
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    
    // 支持以下调度算法：
    // NrMacSchedulerOfdmaPF, NrMacSchedulerOfdmaRR, NrMacSchedulerOfdmaMR, NrMacSchedulerOfdmaQos
    // NrMacSchedulerTdmaPF, NrMacSchedulerTdmaRR, NrMacSchedulerTdmaMR, NrMacSchedulerTdmaQos
    if(scheduler == 0){
        nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));
    }else if(scheduler == 1){
        nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaPF"));
    }else {
        nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaDP"));
    }
    

    // 是否允许重传，默认true
    //nrHelper->SetSchedulerAttribute("EnableHarqReTx", BooleanValue(false));

    Config::SetDefault("ns3::NrRlcUm::EnablePdcpDiscarding", BooleanValue(true));
    Config::SetDefault("ns3::NrRlcUm::DiscardTimerMs", UintegerValue(5));
    
    

    // UNIFORM_POWER_ALLOCATION_USED，在使用的（活动的）RB 上进行均匀功率分配，默认使用此类型
    // UNIFORM_POWER_ALLOCATION_BW：表示在所有带宽（所有 RB）上进行均匀功率分配S
    if(allocate == 0){
        nrHelper->SetGnbPhyAttribute("PowerAllocationType", 
                                 EnumValue(NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_BW));
    nrHelper->SetUePhyAttribute("PowerAllocationType", 
                                EnumValue(NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_BW));
    }
    

    // nrHelper->SetGnbPhyAttribute("PowerAllocationType", 
    //                              EnumValue(NrSpectrumValueHelper::UNIFORM_POWER_ALLOCATION_USED));

    // Scheduler
    nrHelper->SetSchedulerAttribute("FixedMcsDl", BooleanValue(false));
    nrHelper->SetSchedulerAttribute("FixedMcsUl", BooleanValue(false));
    //nrHelper->SetSchedulerAttribute("StartingMcsDl", UintegerValue(5));
    // nrHelper->SetSchedulerAttribute("StartingMcsUl", UintegerValue(2));
    


    uint16_t mcsTable = 2;
    // Error Model: gNB and UE with same spectrum error model.
    std::string errorModel = "ns3::NrEesmIrT" + std::to_string(mcsTable);
    std::string DlerrorModel = "ns3::NrEesmIrT1";
    std::string UlerrorModel = "ns3::NrEesmIrT2";
    nrHelper->SetDlErrorModel(DlerrorModel);
    nrHelper->SetUlErrorModel(UlerrorModel);

    // Both DL and UL AMC will have the same model behind.
    nrHelper->SetGnbDlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));
    nrHelper->SetGnbUlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));


    // Beamforming method
    if (cellScan)
    {
        idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                             TypeIdValue(CellScanBeamforming::GetTypeId()));
        idealBeamformingHelper->SetBeamformingAlgorithmAttribute("BeamSearchAngleStep",
                                                                 DoubleValue(beamSearchAngleStep));
    }
    else
    {
        idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                             TypeIdValue(DirectPathBeamforming::GetTypeId()));
    }


    // 正确设置UE发射功率（全局默认值）
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(23.0)); // 23 dBm


    // call all the setters before initializing the operation band
    nrHelper->InitializeOperationBand(&band);
    allBwps = CcBwpCreator::GetAllBwps({band});

    //double x = pow(10, totalTxPower / 10);

    // Antennas for all the UEs(修改为2x2 MIMO)
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<ThreeGppAntennaModel>()));// 改用3GPP天线模型

    // Antennas for all the gNbs(修改为2x2 MIMO)
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<ThreeGppAntennaModel>()));
    

    uint32_t bwpIdForLowLat = 0;
    uint32_t bwpIdForVoice = 1;
    uint32_t bwpIdForVideo = 2;
    uint32_t bwpIdForVideoGaming = 3;

    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB",
                                                 UintegerValue(bwpIdForLowLat));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_VIDEO_TCP_PREMIUM",
                                                 UintegerValue(bwpIdForVideo));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_VOICE_VIDEO_GAMING",
                                                 UintegerValue(bwpIdForVideoGaming));

    // 设置所有基站的 RBG 大小
    Config::SetDefault("ns3::NrGnbMac::NumRbPerRbg", UintegerValue(1));
    // 例如，设置 numerology 为 1 (对应30 kHz SCS)
    Config::SetDefault("ns3::NrGnbPhy::Numerology", UintegerValue(numerology));
    // 允许上行功率控制
    //Config::SetDefault("ns3::NrUePhy::EnableUplinkPowerControl", BooleanValue(true));
    

    // Install and get the pointers to the NetDevices
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);



    // 设置基站的发射功率
    for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
    {
        Ptr<NrGnbNetDevice> gnb = DynamicCast<NrGnbNetDevice>(*it);
        // 遍历所有组件载波（CC）
        for (uint32_t ccId = 0; ccId < gnb->GetCcMapSize(); ++ccId)
        {
            Ptr<NrGnbPhy> phy = gnb->GetPhy(ccId);
            phy->SetAttribute("TxPower", DoubleValue(totalTxPower)); // 单位dBm
            NS_LOG_UNCOND("gNB " << gnb->GetNode()->GetId() 
                            << " CC " << ccId 
                            << " 发射功率设置为: " << totalTxPower << " dBm"
                            <<"RbNum:"<<phy->GetRbNum()
                            );
        }

    }


    NS_ASSERT(nrHelper != nullptr);

    NS_LOG_UNCOND("Number of gNB NetDevices: " << gnbNetDev.GetN());
    NS_LOG_UNCOND("Number of UE NetDevices: " << ueNetDev.GetN());

    NS_ASSERT(gnbNetDev.GetN() > 0);
    NS_ASSERT(ueNetDev.GetN() > 0);

    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);




    for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }

    NS_LOG_UNCOND("=== 验证 BWP 配置 ===");
    Ptr<NrGnbNetDevice> gnb0 = DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(0));
    uint32_t bwpCount = NrHelper::GetNumberBwp(gnb0);
    NS_LOG_UNCOND("gNB0 实际配置的 BWP 数量: " << bwpCount);



    

    // create the internet and install the IP stack on the UEs
    // get SGW/PGW and create a single RemoteHost
    Ptr<Node> pgw = nrEpcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // connect a remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set the default gateway for the UEs
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
    }


    // attach UEs to the closest gNB before creating the dedicated flows
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);
    
    // 打印基站与用户的绑定关系
    std::map<uint32_t, std::vector<uint32_t>> gnbToUeMap; //维护基站与其UE关系的列表
    NS_LOG_UNCOND("基站与用户的绑定关系:");
    for (uint32_t ueIdx = 0; ueIdx < ueNodes.GetN(); ++ueIdx)
    {
        Ptr<NrUeNetDevice> ueNetDevice = DynamicCast<NrUeNetDevice>(ueNetDev.Get(ueIdx));
        Ptr<const NrGnbNetDevice> servingGnbConst = ueNetDevice->GetTargetGnb();
        Ptr<NrGnbNetDevice> servingGnb = const_cast<NrGnbNetDevice*>(PeekPointer(servingGnbConst));
        if (servingGnb)
        {
            // 找到该 gNB 在 gnbNetDev 中的索引
            uint32_t gnbIdx = 0;
            for (; gnbIdx < gnbNetDev.GetN(); ++gnbIdx)
            {
                if (gnbNetDev.Get(gnbIdx) == servingGnb)
                {
                    break;
                }
            }
            NS_LOG_UNCOND("UE " << ueIdx << " 绑定到 gNB " << gnbIdx);
            gnbToUeMap[gnbIdx].push_back(ueIdx); // 将UE索引添加到对应的基站列表中
        }
        else
        {
            NS_LOG_UNCOND("UE " << ueIdx << " 未绑定到任何 gNB");
        }
    }



   
    // install UDP applications
    uint16_t dlPort = 1234;
    //uint16_t ulPort = dlPort + gNbNum * ueNumPergNb * numFlowsUe + 1;3
    uint16_t ulPort = dlPort + ueNum * numFlowsUe + 1;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        for (uint16_t flow = 0; flow < numFlowsUe; ++flow)
        {
            if (!disableDl)
            {
                PacketSinkHelper dlPacketSinkHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), dlPort));
                serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));

                // 根据用户索引决定包大小
                //uint32_t currentDlPacketSize = (u < ueNodes.GetN() / 2) ? 100 : 5000;
                UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
                dlClient.SetAttribute("PacketSize", UintegerValue(udpPacketSizeDl));// 动态设置包大小
                dlClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaDl)));
                dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                clientApps.Add(dlClient.Install(remoteHost));

                Ptr<NrEpcTft> tft = Create<NrEpcTft>();
                NrEpcTft::PacketFilter dlpf;
                dlpf.localPortStart = dlPort;
                dlpf.localPortEnd = dlPort;
                ++dlPort;
                tft->Add(dlpf);

                enum NrEpsBearer::Qci q;
                if (flow == 0)
                {
                    q = NrEpsBearer::NGBR_LOW_LAT_EMBB;
                }
                else if (flow == 1)
                {
                    q = NrEpsBearer::GBR_CONV_VOICE;
                }
                else if (flow == 2)
                {
                    q = NrEpsBearer::NGBR_VIDEO_TCP_PREMIUM;
                }
                else if (flow == 3)
                {
                    q = NrEpsBearer::NGBR_VOICE_VIDEO_GAMING;
                }
                else
                {
                    q = NrEpsBearer::NGBR_VIDEO_TCP_DEFAULT;
                }
                NrEpsBearer bearer(q);
                nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(u), bearer, tft);
            }

            if (!disableUl)
            {
                PacketSinkHelper ulPacketSinkHelper(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), ulPort));
                serverApps.Add(ulPacketSinkHelper.Install(remoteHost));

                // 根据用户索引决定包大小
                //uint32_t currentUlPacketSize = (u < ueNodes.GetN() / 2) ? 200 : 200;
                UdpClientHelper ulClient(remoteHostAddr, ulPort);
                ulClient.SetAttribute("PacketSize", UintegerValue(udpPacketSizeUl));
                ulClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaUl)));
                ulClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                clientApps.Add(ulClient.Install(ueNodes.Get(u)));

                Ptr<NrEpcTft> tft = Create<NrEpcTft>();
                NrEpcTft::PacketFilter ulpf;
                ulpf.remotePortStart = ulPort;
                ulpf.remotePortEnd = ulPort;
                ++ulPort;
                tft->Add(ulpf);

                enum NrEpsBearer::Qci q;
                if (flow == 0)
                {
                    q = NrEpsBearer::NGBR_LOW_LAT_EMBB;
                }
                else if (flow == 1)
                {
                    q = NrEpsBearer::GBR_CONV_VOICE;
                }
                else if (flow == 2)
                {
                    q = NrEpsBearer::NGBR_VIDEO_TCP_PREMIUM;
                }
                else if (flow == 3)
                {
                    q = NrEpsBearer::NGBR_VOICE_VIDEO_GAMING;
                }
                else
                {
                    q = NrEpsBearer::NGBR_VIDEO_TCP_DEFAULT;
                }
                NrEpsBearer bearer(q);
                nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(u), bearer, tft);
            }
        }
    }

    // enable the traces provided by the nr module 
    // nrHelper->EnableTraces();
    // nrHelper->EnableUlMacSchedTraces();
    // nrHelper->EnableDlDataPhyTraces();
    // nrHelper->EnableUlPhyTraces();
    

    // start UDP server and client apps
    serverApps.Start(Seconds(udpAppStartTime));
    clientApps.Start(Seconds(udpAppStartTime));
    serverApps.Stop(Seconds(simTime));
    clientApps.Stop(Seconds(simTime));


    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueNodes);

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
    //monitor->SetAttribute("MaxPerHopDelay",TimeValue(Seconds(10.0)) );

    // 在 gnbNetDev 和 ueNetDev 安装后添加：
    // 在连接回调函数之前，打印一些调试信息
    NS_LOG_UNCOND("Connecting NotifyRxDataTraceUe callback...");
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/NrUePhy/SpectrumPhy/RxPacketTraceUe",
        MakeCallback(&NotifyRxDataTraceUe));

    NS_LOG_UNCOND("Connecting NotifyRxDataTraceGnb callback...");
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/*/BandwidthPartMap/*/NrGnbPhy/SpectrumPhy/RxPacketTraceGnb",
        MakeCallback(&NotifyRxDataTraceGnb));
        
        

    // 获取仿真开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    Simulator::Stop(Seconds(simTime));
    NS_LOG_UNCOND("仿真开始");
    Simulator::Run();
    // 获取仿真结束时间
    auto endTime = std::chrono::high_resolution_clock::now();

    // 计算仿真运行时间
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    NS_LOG_UNCOND("仿真结束");

    // 在 Simulator::Run() 之后，Simulator::Destroy() 之前添加：
    std::ofstream blerFile(outputDir + "/bler_results.txt");
    WriteRecentBlerStats(outputDir + "/recent_bler_stats.txt");

    blerFile << "gNbNum: " << gNbNum << "\n";
    blerFile << "ueNum: " << ueNum << "\n";
    blerFile << "simTime: " << simTime << " seconds\n";
    blerFile << "totalTxPower: " << totalTxPower << " dBm\n";
    

    // 下行统计
    blerFile << "===== DL BLER Statistics (CellId, RNTI) =====\n";
    for (const auto& entry : g_dlBlerStats) {
        double bler = (entry.second.totalTb == 0) ? 0.0 : 
            static_cast<double>(entry.second.corruptedTb) / entry.second.totalTb;
        double avgMcs = (entry.second.totalTb == 0) ? 0.0 : 
        static_cast<double>(entry.second.totalMcs) / entry.second.totalTb; // 新增
        double avgTbler = (entry.second.totalTb == 0) ? 0.0 : 
        static_cast<double>(entry.second.totalTbler) / entry.second.totalTb; // 新增
        double avgRb = (entry.second.totalTb == 0) ? 0.0 : 
        static_cast<double>(entry.second.totalRb) / entry.second.totalTb; // 新增

        blerFile << "Cell " << entry.first.first 
                << " RNTI " << entry.first.second
                << ": Total=" << entry.second.totalTb 
                << ", Bad=" << entry.second.corruptedTb
                << ", BLER=" << std::fixed << std::setprecision(2) << bler*100 << "%"
                << ", AvgMCS=" << std::fixed << std::setprecision(1) << avgMcs
                << ", AvgTbler=" << std::fixed << std::setprecision(1) << avgTbler
                << ", AvgRB=" << std::fixed << std::setprecision(1) << avgRb
                << "\n";

        // 打印每个MCS值
        const McsKey mcsKey = entry.first;
        const auto& mcsStats = g_dlMcsStats[mcsKey];
        blerFile << "  MCS Values: ";
        for (const auto& mcs : mcsStats.mcsValues) {
            blerFile << static_cast<int>(mcs) << " ";
        }
        blerFile << "\n";
        // 打印每个Rank值
        const TblerKey tblerKey = entry.first;
        const auto& tblerStats = g_dlTblerStats[tblerKey];
        blerFile << "  Tbler Values: ";
        for (const auto& tbler : tblerStats.TblerValues) {
            blerFile << std::fixed << std::setprecision(2) <<tbler*100 << "%" << " ";
        }
        blerFile << "\n";
    }

    // 上行统计
    blerFile << "\n===== UL BLER Statistics (CellId, RNTI) =====\n"; 
    for (const auto& entry : g_ulBlerStats) {
        double bler = (entry.second.totalTb == 0) ? 0.0 : 
            static_cast<double>(entry.second.corruptedTb) / entry.second.totalTb;
        double avgMcs = (entry.second.totalTb == 0) ? 0.0 : 
        static_cast<double>(entry.second.totalMcs) / entry.second.totalTb; // 新增
        double avgRb = (entry.second.totalTb == 0) ? 0.0 : 
        static_cast<double>(entry.second.totalRb) / entry.second.totalTb; // 新增
        blerFile << "Cell " << entry.first.first
                << " RNTI " << entry.first.second
                << ": Total=" << entry.second.totalTb
                << ", Bad=" << entry.second.corruptedTb
                << ", BLER=" << std::fixed << std::setprecision(2) << bler*100 << "%"
                << ", AvgMCS=" << std::fixed << std::setprecision(1) << avgMcs
                << ", AvgRB=" << std::fixed << std::setprecision(1) << avgRb
                << "\n";
    }

    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 转换为时间戳
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    // 转换为本地时间
    std::tm* ltm = std::localtime(&now_c);

    // 格式化为公历时间字符串
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);

    blerFile << "\nTimeStamp: " << buffer<< "\n";
    blerFile << "Simulation Duration: " << duration.count() << " seconds\n"<<"Max DL RB: " << maxDlRb<<","<<"Max UL RB: " << maxUlRb;

    blerFile.close();
    NS_LOG_UNCOND("统计BLER done");

    /*
     * To check what was installed in the memory, i.e., BWPs of gNB Device, and its configuration.
     * Example is: Node 1 -> Device 0 -> BandwidthPartMap -> {0,1} BWPs -> NrGnbPhy ->
    NrPhyMacCommong-> Numerology, Bandwidth, ... GtkConfigStore config; config.ConfigureAttributes
    ();
    */

    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;
    double averagePacketLossRate = 0.0;

    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile.setf(std::ios_base::fixed);

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto "
                << protoStream.str() << "\n";
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
        outFile << "  TxOffered:  "
                << i->second.txBytes * 8.0 / (simTime - udpAppStartTime) / 1000 / 1000 << " Mbps\n";
        outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        if (i->second.rxPackets > 0)
        {
            // Measure the duration of the flow from receiver's perspective
            // double rxDuration = i->second.timeLastRxPacket.GetSeconds () -
            // i->second.timeFirstTxPacket.GetSeconds ();
            double rxDuration = (simTime - udpAppStartTime);

            averageFlowThroughput += i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
            outFile << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
        }
        else
        {
            outFile << "  Throughput:  0 Mbps\n";
            outFile << "  Mean delay:  0 ms\n";
            outFile << "  Mean jitter: 0 ms\n";
        }
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
        // 计算丢包率
        uint32_t lostPackets = i->second.txPackets - i->second.rxPackets;
        double packetLossRate = 0.0;
        if (i->second.txPackets > 0)
        {
            packetLossRate = static_cast<double>(lostPackets) / i->second.txPackets * 100.0;
        }
        averagePacketLossRate += packetLossRate;

        outFile << "  Lost Packets: " << lostPackets << "\n";
        outFile << "  Packet Loss Rate: " << packetLossRate << " %\n";
    }

    outFile << "\n  Mean flow throughput: " << averageFlowThroughput / stats.size() << "Mbps\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "ms\n";
    outFile << " Mean packet loss rate: " << averagePacketLossRate / stats.size() << "%\n";

    // 按小区统计平均BLER等
    double totalDlBler = 0.0;
    uint64_t totalDlMcs = 0; // 新增：下行总MCS值
    uint64_t totalDlRb = 0;  // 新增：下行总RB数
    int dlCellCount = 0;

    // 先按小区分组，计算每个小区内每个用户的平均MCS和平均RB
    std::map<uint16_t, std::vector<double>> dlCellUserAvgMcs;
    std::map<uint16_t, std::vector<double>> dlCellUserAvgRb;

    for (const auto& entry : g_dlBlerStats) {
        const BlerKey& key = entry.first;
        const BlerStats& stats = entry.second;
        uint16_t cellId = key.first;

        if (stats.totalTb > 0) {
            double userAvgMcs = static_cast<double>(stats.totalMcs) / stats.totalTb;
            double userAvgRb = static_cast<double>(stats.totalRb) / stats.totalTb;

            dlCellUserAvgMcs[cellId].push_back(userAvgMcs);
            dlCellUserAvgRb[cellId].push_back(userAvgRb);
        }
    }

    // 再计算每个小区的平均MCS和平均RB
    for (const auto& entry : dlCellUserAvgMcs) {
        uint16_t cellId = entry.first;
        const std::vector<double>& userAvgMcsList = entry.second;
        const std::vector<double>& userAvgRbList = dlCellUserAvgRb[cellId];

        double cellTotalMcs = 0.0;
        double cellTotalRb = 0.0;

        for (double userAvgMcs : userAvgMcsList) {
            cellTotalMcs += userAvgMcs;
        }
        for (double userAvgRb : userAvgRbList) {
            cellTotalRb += userAvgRb;
        }

        double cellAvgMcs = cellTotalMcs / userAvgMcsList.size();
        double cellAvgRb = cellTotalRb / userAvgRbList.size();

        totalDlMcs += cellAvgMcs;
        totalDlRb += cellAvgRb;

        const BlerStats& cellStats = g_dlBlerStatsByCell[cellId];
        if (cellStats.totalTb > 0) {
            totalDlBler += static_cast<double>(cellStats.corruptedTb) / cellStats.totalTb;
        }
        dlCellCount++;
    }



    // for (const auto& entry : g_dlBlerStatsByCell) {
    //     const BlerStats& stats = entry.second;
    //     if (stats.totalTb > 0) {
    //         totalDlBler += static_cast<double>(stats.corruptedTb) / stats.totalTb;
    //         dlCellCount++;
    //     }
    // }
    //outFile <<"totalDlBle"<<  totalDlBler << "\n";
    //outFile <<"dlCellCount"<<  dlCellCount << "\n";
    double averageDlBler = (dlCellCount > 0) ? totalDlBler / dlCellCount*100.0 : 0.0;
    // 计算下行平均MCS值
    double averageDlMcs = (dlCellCount > 0)? static_cast<double>(totalDlMcs) / dlCellCount : 0.0;
    // 计算下行平均RB数
    double averageDlRb = (dlCellCount > 0)? static_cast<double>(totalDlRb) / dlCellCount : 0.0;
    outFile << "Average Downlink BLER by Cell: " << averageDlBler << "%\n";
    // 新增输出下行平均MCS值和平均RB数
    outFile << "Average Downlink MCS by Cell: " << averageDlMcs << "\n";
    outFile << "Average Downlink RB by Cell: " << averageDlRb << "\n";

    double totalUlBler = 0.0;
    uint64_t totalUlMcs = 0; 
    uint64_t totalUlRb = 0; 
    int ulCellCount = 0;

    // 先按小区分组，计算每个小区内每个用户的平均MCS和平均RB
    std::map<uint16_t, std::vector<double>> ulCellUserAvgMcs;
    std::map<uint16_t, std::vector<double>> ulCellUserAvgRb;

    for (const auto& entry : g_ulBlerStats) {
        const BlerKey& key = entry.first;
        const BlerStats& stats = entry.second;
        uint16_t cellId = key.first;

        if (stats.totalTb > 0) {
            double userAvgMcs = static_cast<double>(stats.totalMcs) / stats.totalTb;
            double userAvgRb = static_cast<double>(stats.totalRb) / stats.totalTb;

            ulCellUserAvgMcs[cellId].push_back(userAvgMcs);
            ulCellUserAvgRb[cellId].push_back(userAvgRb);
        }
    }

    // 再计算每个小区的平均MCS和平均RB
    for (const auto& entry : ulCellUserAvgMcs) {
        uint16_t cellId = entry.first;
        const std::vector<double>& userAvgMcsList = entry.second;
        const std::vector<double>& userAvgRbList = ulCellUserAvgRb[cellId];

        double cellTotalMcs = 0.0;
        double cellTotalRb = 0.0;

        for (double userAvgMcs : userAvgMcsList) {
            cellTotalMcs += userAvgMcs;
        }
        for (double userAvgRb : userAvgRbList) {
            cellTotalRb += userAvgRb;
        }

        double cellAvgMcs = cellTotalMcs / userAvgMcsList.size();
        double cellAvgRb = cellTotalRb / userAvgRbList.size();

        totalUlMcs += cellAvgMcs;
        totalUlRb += cellAvgRb;

        const BlerStats& cellStats = g_ulBlerStatsByCell[cellId];
        if (cellStats.totalTb > 0) {
            totalUlBler += static_cast<double>(cellStats.corruptedTb) / cellStats.totalTb;
        }
        ulCellCount++;
    }

    // for (const auto& entry : g_ulBlerStatsByCell) {
    //     const BlerStats& stats = entry.second;
    //     if (stats.totalTb > 0) {
    //         totalUlBler += static_cast<double>(stats.corruptedTb) / stats.totalTb;
    //         ulCellCount++;
    //     }
    // }
    double averageUlBler = (ulCellCount > 0) ? totalUlBler / ulCellCount*100.0 : 0.0;
    double averageUlMcs = (ulCellCount > 0)? static_cast<double>(totalUlMcs) / ulCellCount : 0.0;
    double averageUlRb = (ulCellCount > 0)? static_cast<double>(totalUlRb) / ulCellCount : 0.0;
    outFile << "Average Uplink BLER by Cell: " << averageUlBler << "%\n";
    outFile << "Average Uplink MCS by Cell: " << averageUlMcs << "\n";
    outFile << "Average Uplink RB by Cell: " << averageUlRb << "\n";

    double averageBler = 0.0;
    double averageMcs = 0.0;
    double averageRb = 0.0;
    if(dlOnly == 1){
        averageBler = averageDlBler;
        averageMcs = averageDlMcs;
        averageRb = averageDlRb;
    }else{
        averageBler = averageUlBler;
        averageMcs = averageUlMcs;
        averageRb = averageUlRb;
    }

    outFile << std::fixed << std::setprecision(2);
    outFile << averageFlowThroughput  << "Mbps\n";
    outFile << averageFlowDelay / stats.size() << "ms\n";
    outFile  << averagePacketLossRate / stats.size() << "%\n";
    outFile <<  averageBler << "%\n";
    // 输出全局平均MCS值和平均RB数
    outFile << "Global Average MCS: " << averageMcs << "\n";
    outFile << "Global Average RB: " << averageRb << "\n";
    outFile << "Actual Bandwidth: " << bandwidthBand << " vs Numerology: " << numerology<< "\n";
    


    outFile.close();

    std::ifstream f(filename.c_str());

    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    // 供python脚本捕捉：
    std::cout << "###RESULTS###" 
            << averageFlowThroughput  << ","
            << averageFlowDelay / stats.size() << ","
            << averagePacketLossRate / stats.size()<< ","
            << averageBler<< ","
            <<averageMcs<< ","
            <<averageRb
            << "###END###" << std::endl;

    Simulator::Destroy();
    return 0;
}
