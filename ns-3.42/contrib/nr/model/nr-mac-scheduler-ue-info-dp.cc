// Copyright (c) 2024 Your Name/Organization
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-mac-scheduler-ue-info-dp.h"
#include <ns3/log.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("NrMacSchedulerUeInfoDP");

NrMacSchedulerUeInfoDP::NrMacSchedulerUeInfoDP(float alpha, uint16_t rnti, BeamId beamId, uint16_t cellId, const GetRbPerRbgFn& fn)
    : NrMacSchedulerUeInfoPF(alpha, rnti, beamId, cellId, fn)
{
}

bool NrMacSchedulerUeInfoDP::CompareUeWeightsDl(const ns3::NrMacSchedulerNs3::UePtrAndBufferReq& lue, const ns3::NrMacSchedulerNs3::UePtrAndBufferReq& rue)
{
    auto luePtr = dynamic_cast<NrMacSchedulerUeInfoDP*>(lue.first.get());
    auto ruePtr = dynamic_cast<NrMacSchedulerUeInfoDP*>(rue.first.get());

    // double lMetric = (std::pow(luePtr->m_potentialTputDl, luePtr->m_alpha) / std::max(1E-9, luePtr->m_avgTputDl)) 
    //                 * (1 + luePtr->m_beta * luePtr->GetDlWaitTime());
    
    // double rMetric = (std::pow(ruePtr->m_potentialTputDl, ruePtr->m_alpha) / std::max(1E-9, ruePtr->m_avgTputDl))
    //                 * (1 + ruePtr->m_beta * ruePtr->GetDlWaitTime());

    double lMetric = luePtr->m_beta * luePtr->GetDlWaitTime();
    
    double rMetric =  ruePtr->m_beta * ruePtr->GetDlWaitTime();

    NS_LOG_DEBUG("DL Compare - UE " << luePtr->m_rnti << " Metric: " << lMetric 
                 << " vs UE " << ruePtr->m_rnti << " Metric: " << rMetric);
    return (lMetric > rMetric);
}

bool NrMacSchedulerUeInfoDP::CompareUeWeightsUl(const ns3::NrMacSchedulerNs3::UePtrAndBufferReq& lue, const ns3::NrMacSchedulerNs3::UePtrAndBufferReq& rue)
{
    auto luePtr = dynamic_cast<NrMacSchedulerUeInfoDP*>(lue.first.get());
    auto ruePtr = dynamic_cast<NrMacSchedulerUeInfoDP*>(rue.first.get());

    // double lMetric = (std::pow(luePtr->m_potentialTputUl, luePtr->m_alpha) / std::max(1E-9, luePtr->m_avgTputUl)) 
    //                 * (1 + luePtr->m_beta * luePtr->GetUlWaitTime());
    
    // double rMetric = (std::pow(ruePtr->m_potentialTputUl, ruePtr->m_alpha) / std::max(1E-9, ruePtr->m_avgTputUl))
    //                 * (1 + ruePtr->m_beta * ruePtr->GetUlWaitTime());

    double lMetric = luePtr->m_beta * luePtr->GetUlWaitTime();
    
    double rMetric =  ruePtr->m_beta * ruePtr->GetUlWaitTime();

    NS_LOG_DEBUG("UL Compare - UE " << luePtr->m_rnti << " Metric: " << lMetric 
                 << " vs UE " << ruePtr->m_rnti << " Metric: " << rMetric);
    return (lMetric > rMetric);
}

} // namespace ns3