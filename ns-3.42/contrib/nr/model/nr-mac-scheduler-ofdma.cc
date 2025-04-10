// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#define NS_LOG_APPEND_CONTEXT                                                                      \
    do                                                                                             \
    {                                                                                              \
        std::clog << " [ CellId " << GetCellId() << ", bwpId " << GetBwpId() << "] ";              \
    } while (false);

#include "nr-mac-scheduler-ofdma.h"

#include "nr-fh-control.h"

#include <ns3/log.h>

#include <algorithm>
#include <random>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("NrMacSchedulerOfdma");
NS_OBJECT_ENSURE_REGISTERED(NrMacSchedulerOfdma);

TypeId
NrMacSchedulerOfdma::GetTypeId()
{
    // 创建并返回 TypeId 对象，设置父类为 NrMacSchedulerTdma，添加名为 "SymPerBeam" 的跟踪源
    static TypeId tid =
        TypeId("ns3::NrMacSchedulerOfdma")
            .SetParent<NrMacSchedulerTdma>()
            .AddTraceSource(
                "SymPerBeam",
                "Number of assigned symbol per beam. Gets called every time an assignment is made",
                MakeTraceSourceAccessor(&NrMacSchedulerOfdma::m_tracedValueSymPerBeam),
                "ns3::TracedValueCallback::Uint32");
    return tid;
}

NrMacSchedulerOfdma::NrMacSchedulerOfdma()
    : NrMacSchedulerTdma()
{
    // 构造函数，调用父类 NrMacSchedulerTdma 的构造函数
}

/**
 * 计算每个波束分配的符号数 Calculate the number of symbols to assign to each beam
 * \param symAvail 可用符号数 Number of available symbols
 * \param activeDl 活动的下行链路 UE 及其波束的映射 Map of active DL UE and their beam
 * 每个波束根据其中的 UE 及其需要传输的字节数有不同的传输要求。
 * 对于波束 b，分配的符号数为：sym_b = BufSize(b) * (symAvail / BufSizeTotal)
 * Each beam has a different requirement in terms of byte that should be
 * transmitted with that beam. That requirement depends on the number of UE
 * that are inside such beam, and how many bytes they have to transmit.
 *
 * For the beam \f$ b \f$, the number of assigned symbols is the following:
 *
 * \f$ sym_{b} = BufSize(b) * \frac{symAvail}{BufSizeTotal} \f$
 */
NrMacSchedulerOfdma::BeamSymbolMap
NrMacSchedulerOfdma::GetSymPerBeam(uint32_t symAvail,
                                   const NrMacSchedulerNs3::ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    GetSecond GetUeVector;
    GetSecond GetUeBufSize;
    GetFirst GetBeamId;
    double bufTotal = 0.0;
    uint8_t symUsed = 0;
    BeamSymbolMap ret;

    // Compute buf total 计算总缓冲区大小
    for (const auto& el : activeDl)
    {
        for (const auto& ue : GetUeVector(el))
        {
            bufTotal += GetUeBufSize(ue);
        }
    }

    for (const auto& el : activeDl)
    {
        uint32_t bufSizeBeam = 0;
        for (const auto& ue : GetUeVector(el))
        {
            bufSizeBeam += GetUeBufSize(ue);
        }

        double tmp = symAvail / bufTotal;
        uint32_t symForBeam = static_cast<uint32_t>(bufSizeBeam * tmp);
        symUsed += symForBeam;
        ret.emplace(std::make_pair(GetBeamId(el), symForBeam));
        NS_LOG_DEBUG("Assigned to beam " << GetBeamId(el) << " symbols " << symForBeam);
    }

    NS_ASSERT(symAvail >= symUsed);
    if (symAvail - symUsed > 0)
    {
        uint8_t symToRedistribute = symAvail - symUsed;
        while (symToRedistribute > 0)
        {
            BeamSymbolMap::iterator min = ret.end();
            for (auto it = ret.begin(); it != ret.end(); ++it)
            {
                if (min == ret.end() || it->second < min->second)
                {
                    min = it;
                }
            }
            min->second += 1;
            symToRedistribute--;
            NS_LOG_DEBUG("Assigned to beam "
                         << min->first << " an additional symbol, for a total of " << min->second);
        }
    }

    // Trigger the trace source firing, using const_cast as we don't change
    // the internal state of the class
    // 触发跟踪源，使用 const_cast 因为不会改变类的内部状态
    for (const auto& v : ret)
    {
        const_cast<NrMacSchedulerOfdma*>(this)->m_tracedValueSymPerBeam = v.second;
    }

    return ret;
}

/**
 * \brief Assign the available DL RBG to the UEs 为 UE 分配可用的下行链路 RBG
 * \param symAvail Available symbols
 * \param activeDl Map of active UE and their beams
 * \return a map between beams and the symbol they need 波束及其所需符号的映射
 * 该算法将频率重新分配给波束内的所有 UE。
 * 前提是使用 GetSymPerBeam() 函数计算每个波束的符号数。
 * The algorithm redistributes the frequencies to all the UEs inside a beam.
 * The pre-requisite is to calculate the symbols for each beam, done with
 * the function GetSymPerBeam().
 * The pseudocode is the following (please note that sym_of_beam is a value
 * returned by the GetSymPerBeam() function):
 * <pre>
 * while frequencies > 0: //还有可用的资源
 *    sort (ueVector); //基于某种公平性或优化的资源分配策略对ue排序
 *    ueVector.first().m_dlRBG += 1 * sym_of_beam; //按照排序优先级，为UE的 m_dlRBG 属性增加一定数量的资源
 *    frequencies--; // 可用资源的数量相应减少
 *    UpdateUeDlMetric (ueVector.first());//更新刚刚分配了资源的UE的下行链路指标,如数据传输速率估计、缓冲区状态、延迟等。
 * </pre>
 * 对 UE 进行排序使用 GetUeCompareDlFn() 函数。
 * 该方法包含两个公平性辅助措施：一是避免为缓冲区需求已满足的 UE 分配资源，二是避免在所有 UE 需求都满足时分配符号。
 */
NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    // Iterate through the different beams 遍历不同的波束
    for (const auto& el : activeDl)
    {
        // Distribute the RBG evenly among UEs of the same beam  在同一波束的UE间均匀分配 RBG
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        uint32_t rbgAssignable = 1 * beamSym;
        std::vector<UePtrAndBufferReq> ueVector;
        FTResources assigned(0, 0);
        const std::vector<uint8_t> dlNotchedRBGsMask = GetDlNotchedRbgMask();
        uint32_t resources = !dlNotchedRBGsMask.empty()
                                 ? std::count(dlNotchedRBGsMask.begin(), dlNotchedRBGsMask.end(), 1)
                                 : GetBandwidthInRbg();
        NS_ASSERT(resources > 0);

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
        }

        for (auto& ue : ueVector)
        {
            BeforeDlSched(ue, FTResources(rbgAssignable, beamSym));
        }

        while (resources > 0)
        {
            GetFirst GetUe;
            std::stable_sort(ueVector.begin(), ueVector.end(), GetUeCompareDlFn());
            auto schedInfoIt = ueVector.begin();

            // Ensure fairness: pass over UEs which already has enough resources to transmit 确保公平性：跳过已拥有足够资源进行传输的 UE
            while (schedInfoIt != ueVector.end())
            {
                uint32_t bufQueueSize = schedInfoIt->second;
                if (GetUe(*schedInfoIt)->m_dlTbSize >= std::max(bufQueueSize, 10U))
                {
                    schedInfoIt++;
                }
                else
                {
                    break;
                }
            }

            if (m_nrFhSchedSapProvider)
            {
                if (m_nrFhSchedSapProvider->GetFhControlMethod() ==
                    NrFhControl::FhControlMethod::OptimizeRBs)
                {
                    uint32_t quantizationStep = rbgAssignable;
                    while (schedInfoIt != ueVector.end())
                    {
                        uint32_t maxAssignable = m_nrFhSchedSapProvider->GetMaxRegAssignable(
                            GetBwpId(),
                            GetUe(*schedInfoIt)->m_dlMcs,
                            GetUe(*schedInfoIt)->m_rnti,
                            GetUe(*schedInfoIt)->m_dlRank); // maxAssignable is in REGs
                        // set a minimum of the maxAssignable equal to 5 RBGs 设置 maxAssignable 的最小值为 5 个 RBG
                        maxAssignable = std::max(maxAssignable, 5 * rbgAssignable);

                        // the minimum allocation is one resource in freq, containing rbgAssignable 最小分配是频率上的一个资源，包含 rbgAssignable 个时间资源（REG）
                        // in time (REGs)
                        if (GetUe(*schedInfoIt)->m_dlRBG + quantizationStep > maxAssignable)
                        {
                            schedInfoIt++;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }

            // In the case that all the UE already have their requirements fulfilled,
            // then stop the beam processing and pass to the next
            if (schedInfoIt == ueVector.end())
            {
                break;
            }

            do
            {
                // Assign 1 RBG for each available symbols for the beam, 为波束的每个可用符号分配 1 个 RBG，并更新可用资源计数
                // and then update the count of available resources
                GetUe(*schedInfoIt)->m_dlRBG += rbgAssignable;
                assigned.m_rbg += rbgAssignable;

                GetUe(*schedInfoIt)->m_dlSym = beamSym;
                assigned.m_sym = beamSym;

                resources -= 1; // Resources are RBG, so they do not consider the beamSym

                // Update metrics
                NS_LOG_DEBUG("Assigned " << rbgAssignable << " DL RBG, spanned over " << beamSym
                                         << " SYM, to UE " << GetUe(*schedInfoIt)->m_rnti);
                // 以下调用将根据 UE 报告的秩指示符更新此特定 UE 的 NrMacSchedulerUeInfo 中的 TB 大小。
                // 即使 UE 报告秩指示符为 2，调用一次此方法也足够，因为分配给两个流的 RBG 数量相同。
                // Following call to AssignedDlResources would update the
                // TB size in the NrMacSchedulerUeInfo of this particular UE
                // according the Rank Indicator reported by it. Only one call
                // to this method is enough even if the UE reported rank indicator 2,
                // since the number of RBG assigned to both the streams are the same.
                AssignedDlResources(*schedInfoIt, FTResources(rbgAssignable, beamSym), assigned);
            } while (GetUe(*schedInfoIt)->m_dlTbSize < 10 && resources > 0);

            // Update metrics for the unsuccessful UEs (who did not get any resource in this
            // iteration)
            // 更新未成功获得资源的 UE 的指标
            for (auto& ue : ueVector)
            {
                if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                {
                    NotAssignedDlResources(ue, FTResources(rbgAssignable, beamSym), assigned);
                }
            }
        }

        if (m_nrFhSchedSapProvider)
        {
            if (m_nrFhSchedSapProvider->GetFhControlMethod() ==
                NrFhControl::FhControlMethod::OptimizeMcs)
            {
                GetFirst GetUe;
                auto schedInfoIt = GetUeVector(el).begin();
                while (schedInfoIt != GetUeVector(el).end()) // over all UEs with data 遍历所有有数据的 UE
                {
                    if (GetUe(*schedInfoIt)->m_dlRBG > 0) // UEs with an actual allocation 实际分配了资源的 UE
                    {
                        uint8_t maxMcsAssignable = m_nrFhSchedSapProvider->GetMaxMcsAssignable(
                            GetBwpId(),
                            GetUe(*schedInfoIt)->m_dlRBG,
                            GetUe(*schedInfoIt)->m_rnti,
                            GetUe(*schedInfoIt)->m_dlRank); // max MCS index assignable 可分配的最大 MCS 索引

                        NS_LOG_DEBUG("UE " << GetUe(*schedInfoIt)->m_rnti
                                           << " MCS form sched: " << +GetUe(*schedInfoIt)->m_dlMcs
                                           << " FH max MCS: " << +maxMcsAssignable);

                        GetUe(*schedInfoIt)->m_dlMcs =
                            std::min(GetUe(*schedInfoIt)->m_dlMcs, maxMcsAssignable);
                    }
                    schedInfoIt++;
                }
            }
            if (GetFhControlMethod() == NrFhControl::FhControlMethod::Postponing ||
                GetFhControlMethod() == NrFhControl::FhControlMethod::OptimizeMcs ||
                GetFhControlMethod() == NrFhControl::FhControlMethod::OptimizeRBs)
            {
                GetFirst GetUe;
                std::vector<UePtrAndBufferReq> fhUeVector;
                fhUeVector = ueVector;
                auto rng = std::default_random_engine{};
                std::shuffle(std::begin(fhUeVector), std::end(fhUeVector), rng);
                auto schedInfoIt = fhUeVector.begin();
                while (schedInfoIt != fhUeVector.end())
                {
                    if (GetUe(*schedInfoIt)->m_dlRBG > 0) // UEs with an actual allocation
                    {
                        if (DoesFhAllocationFit(GetBwpId(),
                                                GetUe(*schedInfoIt)->m_dlMcs,
                                                GetUe(*schedInfoIt)->m_dlRBG,
                                                GetUe(*schedInfoIt)->m_dlRank) == 0)
                        {
                            GetUe(*schedInfoIt)->m_dlRBG =
                                0; // remove allocation if the UE does not fit in the available FH
                                   // capacity 如果 UE 不适合可用的 FH 容量，则移除分配
                        }
                    }
                    schedInfoIt++;
                }
            }
        }
    }

    return symPerBeam;
}

NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignULRBG(uint32_t symAvail, const ActiveUeMap& activeUl) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeUl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeUl);

    // Iterate through the different beams
    for (const auto& el : activeUl)
    {
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        uint32_t rbgAssignable = 1 * beamSym;
        std::vector<UePtrAndBufferReq> ueVector;
        FTResources assigned(0, 0);
        const std::vector<uint8_t> ulNotchedRBGsMask = GetUlNotchedRbgMask();
        uint32_t resources = !ulNotchedRBGsMask.empty()
                                 ? std::count(ulNotchedRBGsMask.begin(), ulNotchedRBGsMask.end(), 1)
                                 : GetBandwidthInRbg();
        NS_ASSERT(resources > 0);

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
        }

        for (auto& ue : ueVector)
        {
            BeforeUlSched(ue, FTResources(rbgAssignable, beamSym));
        }

        while (resources > 0)
        {
            GetFirst GetUe;
            std::stable_sort(ueVector.begin(), ueVector.end(), GetUeCompareUlFn());
            auto schedInfoIt = ueVector.begin();

            // Ensure fairness: pass over UEs which already has enough resources to transmit
            while (schedInfoIt != ueVector.end())
            {
                uint32_t bufQueueSize = schedInfoIt->second;
                if (GetUe(*schedInfoIt)->m_ulTbSize >= std::max(bufQueueSize, 12U))
                {
                    schedInfoIt++;
                }
                else
                {
                    break;
                }
            }

            // In the case that all the UE already have their requirements fulfilled,
            // then stop the beam processing and pass to the next
            if (schedInfoIt == ueVector.end())
            {
                break;
            }

            // Assign 1 RBG for each available symbols for the beam,
            // and then update the count of available resources
            GetUe(*schedInfoIt)->m_ulRBG += rbgAssignable;
            assigned.m_rbg += rbgAssignable;

            GetUe(*schedInfoIt)->m_ulSym = beamSym;
            assigned.m_sym = beamSym;

            resources -= 1; // Resources are RBG, so they do not consider the beamSym

            // Update metrics
            NS_LOG_DEBUG("Assigned " << rbgAssignable << " UL RBG, spanned over " << beamSym
                                     << " SYM, to UE " << GetUe(*schedInfoIt)->m_rnti);
            AssignedUlResources(*schedInfoIt, FTResources(rbgAssignable, beamSym), assigned);

            // Update metrics for the unsuccessful UEs (who did not get any resource in this
            // iteration)
            for (auto& ue : ueVector)
            {
                if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                {
                    NotAssignedUlResources(ue, FTResources(rbgAssignable, beamSym), assigned);
                }
            }
        }
    }

    return symPerBeam;
}

/**
 * \brief Create the DL DCI in OFDMA mode 在 OFDMA 模式下创建下行链路 DCI
 * \param spoint Starting point
 * \param ueInfo UE representation
 * \param maxSym Maximum symbols to use 最大可用符号数
 * \return a pointer to the newly created instance
 * 该函数计算 TBS 并调用 CreateDci()
 * The function calculates the TBS and then call CreateDci().
 */
std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateDlDci(NrMacSchedulerNs3::PointInFTPlane* spoint,
                                 const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                 uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    // 计算 TBS Transport Block Size（传输块大小）
    // 根据 ueInfo->m_dlMcs（下行链路的调制编码方案，Modulation and Coding Scheme）、
    // ueInfo->m_dlRank（下行链路的秩，可能与多天线传输的层数有关）和 
    // ueInfo->m_dlRBG * GetNumRbPerRbg()（分配的下行链路资源块组乘以每个资源块组的资源块数量）来计算 TBS。
    uint32_t tbs = m_dlAmc->CalculateTbSize(ueInfo->m_dlMcs,
                                            ueInfo->m_dlRank,
                                            ueInfo->m_dlRBG * GetNumRbPerRbg());
    NS_ASSERT_MSG(ueInfo->m_dlRBG % maxSym == 0,
                  " MaxSym " << maxSym << " RBG: " << ueInfo->m_dlRBG);
    NS_ASSERT(ueInfo->m_dlRBG <= maxSym * GetBandwidthInRbg());
    NS_ASSERT(spoint->m_rbg < GetBandwidthInRbg());
    NS_ASSERT(maxSym <= UINT8_MAX);

    // 5 bytes for headers (3 mac header, 2 rlc header) 
    // 5 字节用于头部（3 字节 MAC 头部，2 字节 RLC 头部）
    if (tbs < 10)
    {
        NS_LOG_DEBUG("While creating DCI for UE " << ueInfo->m_rnti << " assigned "
                                                  << ueInfo->m_dlRBG << " DL RBG, but TBS < 10");
        ueInfo->m_dlTbSize = 0;
        return nullptr;
    }

    uint32_t RBGNum = ueInfo->m_dlRBG / maxSym;
    std::vector<uint8_t> rbgBitmask = GetDlNotchedRbgMask();

    if (rbgBitmask.empty())
    {
        rbgBitmask = std::vector<uint8_t>(GetBandwidthInRbg(), 1);
    }

    // rbgBitmask is all 1s or have 1s in the place we are allowed to transmit.
    // rbgBitmask 全为 1 或在允许传输的位置为 1

    NS_ASSERT(rbgBitmask.size() == GetBandwidthInRbg());

    uint32_t lastRbg = spoint->m_rbg;

    // Limit the places in which we can transmit following the starting point
    // and the number of RBG assigned to the UE
    // 限制可以传输的位置，根据起始点和分配给 UE 的 RBG 数量
    for (uint32_t i = 0; i < GetBandwidthInRbg(); ++i)
    {
        if (i >= spoint->m_rbg && RBGNum > 0 && rbgBitmask[i] == 1)
        {
            // assigned! Decrement RBGNum and continue the for
            // 已分配，减少 RBGNum 并继续循环
            RBGNum--;
            lastRbg = i;
        }
        else
        {
            // Set to 0 the position < spoint->m_rbg OR the remaining RBG when
            // we already assigned the number of requested RBG
            // 将 < spoint->m_rbg 的位置或已分配完所需 RBG 后的剩余位置置为 0
            rbgBitmask[i] = 0;
        }
    }

    NS_ASSERT_MSG(
        RBGNum == 0,
        "If you see this message, it means that the AssignRBG and CreateDci method are unaligned");

    std::ostringstream oss;
    for (const auto& x : rbgBitmask)
    {
        oss << std::to_string(x) << " ";
    }

    NS_LOG_INFO("UE " << ueInfo->m_rnti << " assigned RBG from " << spoint->m_rbg << " with mask "
                      << oss.str() << " for " << static_cast<uint32_t>(maxSym) << " SYM.");

    std::shared_ptr<DciInfoElementTdma> dci =
        std::make_shared<DciInfoElementTdma>(ueInfo->m_rnti,
                                             DciInfoElementTdma::DL,
                                             spoint->m_sym,
                                             maxSym,
                                             ueInfo->m_dlMcs,
                                             ueInfo->m_dlRank,
                                             ueInfo->m_dlPrecMats,
                                             tbs,
                                             1,
                                             0,
                                             DciInfoElementTdma::DATA,
                                             GetBwpId(),
                                             GetTpc());

    dci->m_rbgBitmask = std::move(rbgBitmask);

    NS_ASSERT(std::count(dci->m_rbgBitmask.begin(), dci->m_rbgBitmask.end(), 0) !=
              GetBandwidthInRbg());

    spoint->m_rbg = lastRbg + 1;

    return dci;
}

std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateUlDci(PointInFTPlane* spoint,
                                 const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                 uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    uint32_t tbs = m_ulAmc->CalculateTbSize(ueInfo->m_ulMcs,
                                            ueInfo->m_ulRank,
                                            ueInfo->m_ulRBG * GetNumRbPerRbg());

    // If is less than 12, i.e., 7 (3 mac header, 2 rlc header, 2 data) + 5 bytes for
    // the SHORT_BSR. then we can't transmit any new data, so don't create dci.
    if (tbs < 12)
    {
        NS_LOG_DEBUG("While creating UL DCI for UE " << ueInfo->m_rnti << " assigned "
                                                     << ueInfo->m_ulRBG << " UL RBG, but TBS < 12");
        return nullptr;
    }

    uint32_t RBGNum = ueInfo->m_ulRBG / maxSym;
    std::vector<uint8_t> rbgBitmask = GetUlNotchedRbgMask();

    if (rbgBitmask.empty())
    {
        rbgBitmask = std::vector<uint8_t>(GetBandwidthInRbg(), 1);
    }

    // rbgBitmask is all 1s or have 1s in the place we are allowed to transmit.

    NS_ASSERT(rbgBitmask.size() == GetBandwidthInRbg());

    uint32_t lastRbg = spoint->m_rbg;
    uint32_t assigned = RBGNum;

    // Limit the places in which we can transmit following the starting point
    // and the number of RBG assigned to the UE
    for (uint32_t i = 0; i < GetBandwidthInRbg(); ++i)
    {
        if (i >= spoint->m_rbg && RBGNum > 0 && rbgBitmask[i] == 1)
        {
            // assigned! Decrement RBGNum and continue the for
            RBGNum--;
            lastRbg = i;
        }
        else
        {
            // Set to 0 the position < spoint->m_rbg OR the remaining RBG when
            // we already assigned the number of requested RBG
            rbgBitmask[i] = 0;
        }
    }

    NS_ASSERT_MSG(
        RBGNum == 0,
        "If you see this message, it means that the AssignRBG and CreateDci method are unaligned");

    NS_LOG_INFO("UE " << ueInfo->m_rnti << " assigned RBG from " << spoint->m_rbg << " to "
                      << spoint->m_rbg + assigned << " for " << static_cast<uint32_t>(maxSym)
                      << " SYM.");

    NS_ASSERT(spoint->m_sym >= maxSym);
    std::shared_ptr<DciInfoElementTdma> dci =
        std::make_shared<DciInfoElementTdma>(ueInfo->m_rnti,
                                             DciInfoElementTdma::UL,
                                             spoint->m_sym - maxSym,
                                             maxSym,
                                             ueInfo->m_ulMcs,
                                             ueInfo->m_ulRank,
                                             ueInfo->m_ulPrecMats,
                                             tbs,
                                             1,
                                             0,
                                             DciInfoElementTdma::DATA,
                                             GetBwpId(),
                                             GetTpc());

    dci->m_rbgBitmask = std::move(rbgBitmask);

    std::ostringstream oss;
    for (auto x : dci->m_rbgBitmask)
    {
        oss << std::to_string(x) << " ";
    }
    NS_LOG_INFO("UE " << ueInfo->m_rnti << " DCI RBG mask: " << oss.str());

    NS_ASSERT(std::count(dci->m_rbgBitmask.begin(), dci->m_rbgBitmask.end(), 0) !=
              GetBandwidthInRbg());

    spoint->m_rbg = lastRbg + 1;

    return dci;
}

void
NrMacSchedulerOfdma::ChangeDlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_rbg = 0;
    spoint->m_sym += symOfBeam;
}

void
NrMacSchedulerOfdma::ChangeUlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_rbg = 0;
    spoint->m_sym -= symOfBeam;
}

uint8_t
NrMacSchedulerOfdma::GetTpc() const
{
    NS_LOG_FUNCTION(this);
    return 1; // 1 is mapped to 0 for Accumulated mode, and to -1 in Absolute mode TS38.213 Table
              // Table 7.1.1-1
}

} // namespace ns3
