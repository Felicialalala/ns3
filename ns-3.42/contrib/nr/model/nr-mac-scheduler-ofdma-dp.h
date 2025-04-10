// Copyright (c) 2024 Your Name/Organization
// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include "nr-mac-scheduler-ofdma-pf.h"

namespace ns3 {

class NrMacSchedulerOfdmaDP : public NrMacSchedulerOfdmaPF
{
  public:
    static TypeId GetTypeId();
    NrMacSchedulerOfdmaDP();

    void SetBeta(double beta);
    double GetBeta() const;

  protected:
    std::shared_ptr<NrMacSchedulerUeInfo> CreateUeRepresentation(
        const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const override;

    std::function<bool(const UePtrAndBufferReq&, const UePtrAndBufferReq&)>
    GetUeCompareDlFn() const override;

    std::function<bool(const UePtrAndBufferReq&, const UePtrAndBufferReq&)>
    GetUeCompareUlFn() const override;

    void AssignedDlResources(const UePtrAndBufferReq& ue, const FTResources& assigned,
                             const FTResources& totAssigned) const override;
    void AssignedUlResources(const UePtrAndBufferReq& ue, const FTResources& assigned,
                             const FTResources& totAssigned) const override;

    void DoSchedDlTriggerReq(const NrMacSchedSapProvider::SchedDlTriggerReqParameters& params) override;
    void DoSchedUlTriggerReq(const NrMacSchedSapProvider::SchedUlTriggerReqParameters& params) override;

  private:
    double m_beta{0.1}; // 时延权重系数
};

} // namespace ns3