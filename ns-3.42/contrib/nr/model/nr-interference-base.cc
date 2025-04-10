// Copyright (c) 2009 CTTC
//
// SPDX-License-Identifier: GPL-2.0-only
//
// Author: Nicola Baldo <nbaldo@cttc.es>

#include "nr-interference-base.h"

#include "nr-chunk-processor.h"

#include <ns3/log.h>
#include <ns3/simulator.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrInterferenceBase");

NrInterferenceBase::NrInterferenceBase()
    : m_receiving(false),
      m_lastSignalId(0),
      m_lastSignalIdBeforeReset(0)
{
    NS_LOG_FUNCTION(this);
}

NrInterferenceBase::~NrInterferenceBase()
{
    NS_LOG_FUNCTION(this);
}

void
NrInterferenceBase::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_rsPowerChunkProcessorList.clear();
    m_sinrChunkProcessorList.clear();
    m_interfChunkProcessorList.clear();
    m_rxSignal = nullptr;
    m_allSignals = nullptr;
    m_noise = nullptr;
    Object::DoDispose();
}

TypeId
NrInterferenceBase::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrInterferenceBase").SetParent<Object>().SetGroupName("Nr");
    return tid;
}

void
NrInterferenceBase::StartRx(Ptr<const SpectrumValue> rxPsd)
{
    NS_LOG_FUNCTION(this << *rxPsd);
    if (!m_receiving)
    {
        NS_LOG_LOGIC("first signal");
        m_rxSignal = rxPsd->Copy();
        m_lastChangeTime = Now();
        m_receiving = true;
        for (auto it = m_rsPowerChunkProcessorList.begin(); it != m_rsPowerChunkProcessorList.end();
             ++it)
        {
            (*it)->Start();
        }
        for (auto it = m_interfChunkProcessorList.begin(); it != m_interfChunkProcessorList.end();
             ++it)
        {
            (*it)->Start();
        }
        for (auto it = m_sinrChunkProcessorList.begin(); it != m_sinrChunkProcessorList.end(); ++it)
        {
            (*it)->Start();
        }
    }
    else
    {
        NS_LOG_LOGIC("additional signal" << *m_rxSignal);
        // receiving multiple simultaneous signals, make sure they are synchronized
        NS_ASSERT(m_lastChangeTime == Now());
        // make sure they use orthogonal resource blocks
        NS_ASSERT(Sum((*rxPsd) * (*m_rxSignal)) == 0.0);
        (*m_rxSignal) += (*rxPsd);
    }
}

void
NrInterferenceBase::EndRx()
{
    NS_LOG_FUNCTION(this);
    if (!m_receiving)
    {
        NS_LOG_INFO("EndRx was already evaluated or RX was aborted");
    }
    else
    {
        ConditionallyEvaluateChunk();
        m_receiving = false;
        for (auto it = m_rsPowerChunkProcessorList.begin(); it != m_rsPowerChunkProcessorList.end();
             ++it)
        {
            (*it)->End();
        }
        for (auto it = m_interfChunkProcessorList.begin(); it != m_interfChunkProcessorList.end();
             ++it)
        {
            (*it)->End();
        }
        for (auto it = m_sinrChunkProcessorList.begin(); it != m_sinrChunkProcessorList.end(); ++it)
        {
            (*it)->End();
        }
    }
}

void
NrInterferenceBase::AddSignal(Ptr<const SpectrumValue> spd, const Time duration)
{
    NS_LOG_FUNCTION(this << *spd << duration);
    DoAddSignal(spd);
    uint32_t signalId = ++m_lastSignalId;
    if (signalId == m_lastSignalIdBeforeReset)
    {
        // This happens when m_lastSignalId eventually wraps around. Given that so
        // many signals have elapsed since the last reset, we hope that by now there is
        // no stale pending signal (i.e., a signal that was scheduled
        // for subtraction before the reset). So we just move the
        // boundary further.
        m_lastSignalIdBeforeReset += 0x10000000;
    }
    Simulator::Schedule(duration, &NrInterferenceBase::DoSubtractSignal, this, spd, signalId);
}

void
NrInterferenceBase::DoAddSignal(Ptr<const SpectrumValue> spd)
{
    NS_LOG_FUNCTION(this << *spd);
    // 在添加新信号之前，先调用 ConditionallyEvaluateChunk() 函数，确保在信号发生变化时，能及时评估之前时间段内的信号块，计算干扰和信噪比等参数。
    ConditionallyEvaluateChunk();
    (*m_allSignals) += (*spd);
}

void
NrInterferenceBase::DoSubtractSignal(Ptr<const SpectrumValue> spd, uint32_t signalId)
{
    NS_LOG_FUNCTION(this << *spd);
    // 在移除信号之前，同样需要先评估之前时间段内的信号块，因为信号的移除会导致信号的功率谱密度发生变化,改变信号的整体状态。
    ConditionallyEvaluateChunk();
    int32_t deltaSignalId = signalId - m_lastSignalIdBeforeReset;
    if (deltaSignalId > 0)
    {
        (*m_allSignals) -= (*spd);
    }
    else
    {
        NS_LOG_INFO("ignoring signal scheduled for subtraction before last reset");
    }
}

/*
 * 此函数会检查是否正在接收信号（m_receiving 为 true），并且当前时间是否超过上一次信号变化时间（Now() > m_lastChangeTime）。若满足条件，就会进行以下操作：
 * 计算干扰功率谱密度 interf，其值为所有信号功率谱密度减去当前接收信号功率谱密度再加上噪声功率谱密度。
 * 计算信噪比 sinr，即当前接收信号功率谱密度除以干扰功率谱密度。
 * 计算信号块持续时间 duration。
 * 分别调用 m_sinrChunkProcessorList、m_interfChunkProcessorList 和 m_rsPowerChunkProcessorList 中的处理器，对计算得到的 sinr、interf 和 *m_rxSignal 进行处理。
 * 更新 m_lastChangeTime 为当前时间。
 */
void
NrInterferenceBase::ConditionallyEvaluateChunk()
{
    NS_LOG_FUNCTION(this);
    if (m_receiving)
    {
        NS_LOG_DEBUG(this << " Receiving");
    }
    NS_LOG_DEBUG(this << " now " << Now() << " last " << m_lastChangeTime);
    if (m_receiving && (Now() > m_lastChangeTime))
    {
        NS_LOG_LOGIC(this << " signal = " << *m_rxSignal << " allSignals = " << *m_allSignals
                          << " noise = " << *m_noise);

        // m_allSignals 表示所有信号的功率谱密度，m_rxSignal 表示当前正在接收的信号的功率谱密度，m_noise 表示噪声的功率谱密度。
        // 通过计算 m_allSignals - m_rxSignal + m_noise，可以得到所有信号的干扰功率谱密度。
        SpectrumValue interf = (*m_allSignals) - (*m_rxSignal) + (*m_noise);

        // 根据 SINR 的定义，将当前接收信号的功率谱密度 (*m_rxSignal) 除以总的干扰加噪声的功率谱密度 interf，得到 SINR 的功率谱密度 sinr
        SpectrumValue sinr = (*m_rxSignal) / interf;
        Time duration = Now() - m_lastChangeTime;
        // 这是一个存储 NrChunkProcessor 类型指针的列表，每个 NrChunkProcessor 对象都有处理信号块的能力。
        for (auto it = m_sinrChunkProcessorList.begin(); it != m_sinrChunkProcessorList.end(); ++it)
        {
            // 通过遍历 m_sinrChunkProcessorList，调用每个处理器对象的 EvaluateChunk 方法，并将计算得到的 sinr 和信号块持续时间 duration 作为参数传递进去。
            (*it)->EvaluateChunk(sinr, duration);
        }
        for (auto it = m_interfChunkProcessorList.begin(); it != m_interfChunkProcessorList.end();
             ++it)
        {
            (*it)->EvaluateChunk(interf, duration);
        }
        for (auto it = m_rsPowerChunkProcessorList.begin(); it != m_rsPowerChunkProcessorList.end();
             ++it)
        {
            (*it)->EvaluateChunk(*m_rxSignal, duration);
        }
        m_lastChangeTime = Now();
    }
}

void
NrInterferenceBase::SetNoisePowerSpectralDensity(Ptr<const SpectrumValue> noisePsd)
{
    NS_LOG_FUNCTION(this << *noisePsd);
    // 当设置噪声功率谱密度时，噪声的变化会影响干扰和信噪比的计算。
    // 因此，在更新噪声功率谱密度之前，先调用 ConditionallyEvaluateChunk() 函数，对之前时间段内的信号块进行评估。
    ConditionallyEvaluateChunk();
    m_noise = noisePsd;
    // reset m_allSignals (will reset if already set previously)
    // this is needed since this method can potentially change the SpectrumModel
    m_allSignals = Create<SpectrumValue>(noisePsd->GetSpectrumModel());
    if (m_receiving)
    {
        // abort rx
        m_receiving = false;
    }
    // record the last SignalId so that we can ignore all signals that
    // were scheduled for subtraction before m_allSignal
    m_lastSignalIdBeforeReset = m_lastSignalId;
}

void
NrInterferenceBase::AddRsPowerChunkProcessor(Ptr<NrChunkProcessor> p)
{
    NS_LOG_FUNCTION(this << p);
    m_rsPowerChunkProcessorList.push_back(p);
}

void
NrInterferenceBase::AddSinrChunkProcessor(Ptr<NrChunkProcessor> p)
{
    NS_LOG_FUNCTION(this << p);
    m_sinrChunkProcessorList.push_back(p);
}

void
NrInterferenceBase::AddInterferenceChunkProcessor(Ptr<NrChunkProcessor> p)
{
    NS_LOG_FUNCTION(this << p);
    m_interfChunkProcessorList.push_back(p);
}

} // namespace ns3
