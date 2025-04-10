// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef NR_SPECTRUM_VALUE_HELPER_H
#define NR_SPECTRUM_VALUE_HELPER_H

#include <ns3/spectrum-value.h>

#include <vector>

namespace ns3
{

/**
 * \ingroup spectrum
 *
 * \brief This class provides a set of useful functions when working with spectrum model for NR
 */
class NrSpectrumValueHelper
{
  public:
    enum PowerAllocationType
    {
        UNIFORM_POWER_ALLOCATION_USED,
        UNIFORM_POWER_ALLOCATION_BW,
        CUSTOM_PER_RB_POWER_ALLOCATION//新增，以RB为单位进行自定义功率分配
    };

    static const uint8_t SUBCARRIERS_PER_RB = 12; //!< subcarriers per resource block

    /**
     * \brief Creates or obtains from a global map a spectrum model with a given number of RBs,
     * center frequency and subcarrier spacing.
     * \param numRbs bandwidth in number of RBs
     * \param centerFrequency the center frequency of this band
     * \return pointer to a spectrum model with defined characteristics
     */
    static Ptr<const SpectrumModel> GetSpectrumModel(uint32_t numRbs,
                                                     double centerFrequency,
                                                     double subcarrierSpacing);

    /**
     * \brief Create SpectrumValue that will represent transmit power spectral density,
     * and assuming that all RBs are active.
     * \param powerTx total power in dBm
     * \param rbIndexVector the list of active/used RBs for the current transmission
     * \param txSm spectrumModel to be used to create this SpectrumValue
     * \param allocationType power allocation type to be used
     * \return spectrum value representing power spectral density for given parameters
     */
    static Ptr<SpectrumValue> CreateTxPowerSpectralDensity(double powerTx,
                                                           const std::vector<int>& rbIndexVector,
                                                           const Ptr<const SpectrumModel>& txSm,
                                                           enum PowerAllocationType allocationType);


    /** 新增
     * \brief Create SpectrumValue with per-RB power defined in dBm
     * \param powerPerRbDbm Vector of power values per RB in dBm
     * \param spectrumModel Spectrum model to use
     * \param allocationType Must be CUSTOM_PER_RB_POWER_ALLOCATION
     * \return SpectrumValue with power spectral density per RB
     */
    static Ptr<SpectrumValue> CreateTxPowerSpectralDensity(
      const std::vector<double>& powerPerRbDbm,
      const Ptr<const SpectrumModel>& spectrumModel,
      enum PowerAllocationType allocationType);

    /**
     * \brief Create a SpectrumValue that models the power spectral density of AWGN
     * \param noiseFigure the noise figure in dB  w.r.t. a reference temperature of 290K
     * \param spectrumModel the SpectrumModel instance to be used to create the output spectrum
     * value \return a pointer to a newly allocated SpectrumValue representing the noise Power
     * Spectral Density in W/Hz for each Resource Block
     */
    static Ptr<SpectrumValue> CreateNoisePowerSpectralDensity(
        double noiseFigure,
        const Ptr<const SpectrumModel>& spectrumModel);

    /**
     * \brief Returns the effective bandwidth for the total system bandwidth
     * \param bandwidth the total system bandwidth in Hz
     * \param numerology the numerology to be used over the whole bandwidth
     * \return effective bandwidth which is the sum of bandwidths of all sub-bands, in Hz
     */
    static uint64_t GetEffectiveBandwidth(double bandwidth, uint8_t numerology);

  protected:
    /**
     * \brief Create SpectrumValue that will represent transmit power spectral density, and
     * the total transmit power will be uniformly distributed only over active RBs
     * \param powerTx total power in dBm
     * \param activeRbs vector of RBs that are active for this transmission
     * \param spectrumModel spectrumModel to be used to create this SpectrumValue
     */
    static Ptr<SpectrumValue> CreateTxPsdOverActiveRbs(
        double powerTx,
        const std::vector<int>& activeRbs,
        const Ptr<const SpectrumModel>& spectrumModel);

    /**
     * \brief Create SpectrumValue that will represent transmit power spectral density, and
     * the total transmit power will divided among all RBs, and then it will be assigned to active
     * RBs \param powerTx total power in dBm \param activeRbs vector of RBs that are active for this
     * transmission \param spectrumModel spectrumModel to be used to create this SpectrumValue
     */
    static Ptr<SpectrumValue> CreateTxPsdOverAllRbs(double powerTx,
                                                    const std::vector<int>& activeRbs,
                                                    const Ptr<const SpectrumModel>& spectrumModel);

        /**新增
     * \brief Create SpectrumValue with power per RB specified individually (in dBm)
     * \param powerPerRbDbm power values in dBm per RB, size should be <= number of RBs in the model
     * \param spectrumModel SpectrumModel to build the SpectrumValue on
     * \return SpectrumValue with per-RB power density (W/Hz)
     */
    static Ptr<SpectrumValue> CreateTxPsdPerRbPower(
      const std::vector<double>& powerPerRbDbm,
      const Ptr<const SpectrumModel>& spectrumModel);


    /**
     * Delete SpectrumValues stored in g_nrSpectrumModelMap
     */
    static void DeleteSpectrumValues();
};

} // namespace ns3

#endif /*  NR_SPECTRUM_VALUE_HELPER_H */
