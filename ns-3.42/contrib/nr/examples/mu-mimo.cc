#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/config-store.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("NrHelper", LOG_LEVEL_INFO);

    // Create NR and EPC Helpers
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Configure the beamforming helper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    // Configure spectrum and operational band
    BandwidthPartInfo::Scenario scenario = BandwidthPartInfo::UMi_StreetCanyon;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    double centralFrequency = 28e9;
    double bandwidth = 100e6;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, numCcPerBand, scenario);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    nrHelper->InitializeOperationBand(&band);

    // Set bandwidth part manager attributes
    uint32_t bwpId = 0;
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB", UintegerValue(bwpId));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB", UintegerValue(bwpId));
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    // Create nodes
    NodeContainer gnbNodes, ueNodes;
    gnbNodes.Create(3);
    ueNodes.Create(3);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 10.0));
    positionAlloc->Add(Vector(20.0, 0.0, 1.5));
    positionAlloc->Add(Vector(500.0, 0.0, 10.0));
    positionAlloc->Add(Vector(520.0, 0.0, 1.5));
    positionAlloc->Add(Vector(1000.0, 0.0, 10.0));
    positionAlloc->Add(Vector(1020.0, 0.0, 1.5));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    // Install Internet stack on remote host and UEs
    Ptr<Node> pgw = nrEpcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    // Connect remoteHost to pgw and setup routing
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install NR devices
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Debug: Check the number of devices installed
    NS_LOG_UNCOND("gNB Devices: " << gnbNetDev.GetN());
    NS_LOG_UNCOND("UE Devices: " << ueNetDev.GetN());
    NS_ASSERT_MSG(gnbNetDev.GetN() >= gnbNodes.GetN(), "Not enough gNB devices installed");
    NS_ASSERT_MSG(ueNetDev.GetN() >= ueNodes.GetN(), "Not enough UE devices installed");

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface = nrEpcHelper->AssignUeIpv4Address(ueNetDev);

    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
    }
    NS_LOG_UNCOND("line 103");
    NS_LOG_UNCOND(ueNodes.GetN());

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        NS_LOG_UNCOND("Attaching UE " << i << " to gNB " << i);
    }
    // Attach UEs to gNBs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        NS_LOG_UNCOND("Attaching UE " << i << " to gNB " << i);
        // Debug: Ensure both UE and gNB devices are valid
        Ptr<NetDevice> ueDevice = ueNetDev.Get(i);
        Ptr<NetDevice> gnbDevice = gnbNetDev.Get(i);

        NS_ASSERT_MSG(ueDevice != nullptr, "UE device is null");
        NS_ASSERT_MSG(gnbDevice != nullptr, "gNB device is null");

        // Attach UE to corresponding gNB
        nrHelper->AttachToGnb(ueDevice, gnbDevice);
    }
    NS_LOG_UNCOND("line 119");

    // Configure traffic
    uint16_t dlPort = 1234;
    ApplicationContainer serverApps;
    UdpServerHelper dlPacketSink(dlPort);
    serverApps.Add(dlPacketSink.Install(ueNodes));
    NS_LOG_UNCOND("line 111");

    UdpClientHelper dlClient(internetIpIfaces.GetAddress(1), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    ApplicationContainer clientApps = dlClient.Install(remoteHost);
    NS_LOG_UNCOND("line 118");

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
