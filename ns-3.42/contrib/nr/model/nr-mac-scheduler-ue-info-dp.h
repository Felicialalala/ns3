// Copyright (c) 2024 Your Name/Organization
// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include "nr-mac-scheduler-ue-info-pf.h"

namespace ns3 {

class NrMacSchedulerUeInfoDP : public NrMacSchedulerUeInfoPF
{
  public:
    NrMacSchedulerUeInfoDP(float alpha, uint16_t rnti, BeamId beamId, uint16_t cellId, const GetRbPerRbgFn& fn);
    
    // 时延相关方法
    void IncrementDlWaitTime() { m_dlWaitTime++; }
    void IncrementUlWaitTime() { m_ulWaitTime++; }
    void ResetDlWaitTime() { m_dlWaitTime = 0; }
    void ResetUlWaitTime() { m_ulWaitTime = 0; }
    uint64_t GetDlWaitTime() const { return m_dlWaitTime; }
    uint64_t GetUlWaitTime() const { return m_ulWaitTime; }

    float m_beta{0.0};  

    // 优先级比较函数
    static bool CompareUeWeightsDl(const NrMacSchedulerNs3::UePtrAndBufferReq& lue,
                                   const NrMacSchedulerNs3::UePtrAndBufferReq& rue);
    static bool CompareUeWeightsUl(const NrMacSchedulerNs3::UePtrAndBufferReq& lue,
                                   const NrMacSchedulerNs3::UePtrAndBufferReq& rue);

  private:
    uint64_t m_dlWaitTime{0};  // 下行等待时间（调度周期数）
    uint64_t m_ulWaitTime{0};  // 上行等待时间

    
};

} // namespace ns3