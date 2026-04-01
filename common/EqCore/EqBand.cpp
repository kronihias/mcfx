/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "EqBand.h"

EqBand::EqBand()
{
}

EqBand::~EqBand()
{
}

void EqBand::setType(EqBandType t)
{
    type_ = t;
    reset();
}

void EqBand::setIIRSubType(IIRSubType st)
{
    iirSubType_ = st;
    hasRawCoeffs_ = false;
    updateIIRCoefficients();
}

void EqBand::setFrequency(float f)
{
    frequency_ = f;
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setQ(float q)
{
    q_ = q;
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setGainDB(float db)
{
    gainDB_ = db;
    linearGain_ = Decibels::decibelsToGain(db);
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setFIRCoefficients(const std::vector<float>& coeffs)
{
    firCoeffs_ = coeffs;
    firState_.resize(coeffs.size(), 0.f);
}

void EqBand::setRawCoefficients(float b0, float b1, float b2, float a0, float a1, float a2)
{
    hasRawCoeffs_ = true;
    rawCoeffs_ = { b0, b1, b2, a0, a1, a2 };
    // Normalize by a0
    if (a0 != 0.f && a0 != 1.f)
    {
        rawCoeffs_.b0 /= a0;
        rawCoeffs_.b1 /= a0;
        rawCoeffs_.b2 /= a0;
        rawCoeffs_.a1 /= a0;
        rawCoeffs_.a2 /= a0;
        rawCoeffs_.a0 = 1.f;
    }
    iirCoeffs_ = IIRCoefficients(rawCoeffs_.b0, rawCoeffs_.b1, rawCoeffs_.b2,
                                  1.f, rawCoeffs_.a1, rawCoeffs_.a2);
}

void EqBand::setDelaySamples(int samples)
{
    delaySamples_ = samples;
    if (delaySamples_ > 0)
    {
        delayBuffer_.resize(delaySamples_, 0.f);
        delayWritePos_ = 0;
    }
}

void EqBand::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;

    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();

    if (type_ == EqBandType::FIR)
        firState_.resize(firCoeffs_.size(), 0.f);

    if (type_ == EqBandType::Delay && delaySamples_ > 0)
    {
        delayBuffer_.resize(delaySamples_, 0.f);
        delayWritePos_ = 0;
    }

    reset();
}

void EqBand::processBlock(float* data, int numSamples)
{
    if (!enabled_)
        return;

    switch (type_)
    {
        case EqBandType::IIR:   applyIIR(data, numSamples); break;
        case EqBandType::FIR:   applyFIR(data, numSamples); break;
        case EqBandType::Gain:  applyGain(data, numSamples); break;
        case EqBandType::Delay: applyDelay(data, numSamples); break;
    }
}

void EqBand::reset()
{
    iirState_[0] = 0.f;
    iirState_[1] = 0.f;
    std::fill(firState_.begin(), firState_.end(), 0.f);
    std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.f);
    delayWritePos_ = 0;
}

void EqBand::updateIIRCoefficients()
{
    float gain = Decibels::decibelsToGain(gainDB_);
    float f = jlimit(20.f, (float)(sampleRate_ * 0.499), frequency_);

    switch (iirSubType_)
    {
        case IIRSubType::LowPass:
            iirCoeffs_ = IIRCoefficients::makeLowPass(sampleRate_, f, q_);
            break;
        case IIRSubType::HighPass:
            iirCoeffs_ = IIRCoefficients::makeHighPass(sampleRate_, f, q_);
            break;
        case IIRSubType::BandPass:
            iirCoeffs_ = IIRCoefficients::makeBandPass(sampleRate_, f, q_);
            break;
        case IIRSubType::Notch:
            iirCoeffs_ = IIRCoefficients::makeNotchFilter(sampleRate_, f, q_);
            break;
        case IIRSubType::AllPass:
            iirCoeffs_ = IIRCoefficients::makeAllPass(sampleRate_, f, q_);
            break;
        case IIRSubType::LowShelf:
            iirCoeffs_ = IIRCoefficients::makeLowShelf(sampleRate_, f, q_, gain);
            break;
        case IIRSubType::HighShelf:
            iirCoeffs_ = IIRCoefficients::makeHighShelf(sampleRate_, f, q_, gain);
            break;
        case IIRSubType::Peak:
            iirCoeffs_ = IIRCoefficients::makePeakFilter(sampleRate_, f, q_, gain);
            break;
    }
}

void EqBand::applyIIR(float* data, int numSamples)
{
    auto* c = iirCoeffs_.coefficients; // b0, b1, b2, a1, a2
    float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];

    for (int i = 0; i < numSamples; ++i)
    {
        float x = data[i];
        float y = b0 * x + iirState_[0];
        iirState_[0] = b1 * x - a1 * y + iirState_[1];
        iirState_[1] = b2 * x - a2 * y;
        data[i] = y;
    }
}

void EqBand::applyFIR(float* data, int numSamples)
{
    if (firCoeffs_.empty())
        return;

    int firLen = (int)firCoeffs_.size();

    for (int i = 0; i < numSamples; ++i)
    {
        // Shift state
        for (int j = firLen - 1; j > 0; --j)
            firState_[j] = firState_[j - 1];
        firState_[0] = data[i];

        float y = 0.f;
        for (int j = 0; j < firLen; ++j)
            y += firCoeffs_[j] * firState_[j];

        data[i] = y;
    }
}

void EqBand::applyGain(float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        data[i] *= linearGain_;
}

void EqBand::applyDelay(float* data, int numSamples)
{
    if (delaySamples_ <= 0 || delayBuffer_.empty())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = data[i];
        int readPos = delayWritePos_ - delaySamples_;
        if (readPos < 0)
            readPos += delaySamples_;
        data[i] = delayBuffer_[readPos];
        delayBuffer_[delayWritePos_] = x;
        delayWritePos_ = (delayWritePos_ + 1) % delaySamples_;
    }
}

std::complex<float> EqBand::getFrequencyResponse(double freqHz) const
{
    if (!enabled_)
        return std::complex<float>(1.f, 0.f);

    switch (type_)
    {
        case EqBandType::IIR:
        {
            // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
            auto* c = iirCoeffs_.coefficients;
            float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];

            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            std::complex<double> z_inv(cos(-omega), sin(-omega));
            std::complex<double> z_inv2 = z_inv * z_inv;

            std::complex<double> num = (double)b0 + (double)b1 * z_inv + (double)b2 * z_inv2;
            std::complex<double> den = 1.0 + (double)a1 * z_inv + (double)a2 * z_inv2;

            auto resp = num / den;
            return std::complex<float>((float)resp.real(), (float)resp.imag());
        }

        case EqBandType::FIR:
        {
            // H(z) = sum(h[n] * z^-n)
            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            std::complex<double> sum(0.0, 0.0);
            for (int n = 0; n < (int)firCoeffs_.size(); ++n)
            {
                double angle = -omega * n;
                sum += (double)firCoeffs_[n] * std::complex<double>(cos(angle), sin(angle));
            }
            return std::complex<float>((float)sum.real(), (float)sum.imag());
        }

        case EqBandType::Gain:
            return std::complex<float>(linearGain_, 0.f);

        case EqBandType::Delay:
        {
            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            double angle = -omega * delaySamples_;
            return std::complex<float>((float)cos(angle), (float)sin(angle));
        }
    }

    return std::complex<float>(1.f, 0.f);
}

// --- JSON serialization ---

static String iirSubTypeToString(IIRSubType st)
{
    switch (st)
    {
        case IIRSubType::LowPass:   return "low_pass";
        case IIRSubType::HighPass:  return "high_pass";
        case IIRSubType::BandPass:  return "band_pass";
        case IIRSubType::Notch:     return "notch";
        case IIRSubType::AllPass:   return "all_pass";
        case IIRSubType::LowShelf:  return "low_shelf";
        case IIRSubType::HighShelf: return "high_shelf";
        case IIRSubType::Peak:      return "peak";
    }
    return "peak";
}

static IIRSubType stringToIIRSubType(const String& s)
{
    if (s == "low_pass")   return IIRSubType::LowPass;
    if (s == "high_pass")  return IIRSubType::HighPass;
    if (s == "band_pass")  return IIRSubType::BandPass;
    if (s == "notch")      return IIRSubType::Notch;
    if (s == "all_pass")   return IIRSubType::AllPass;
    if (s == "low_shelf")  return IIRSubType::LowShelf;
    if (s == "high_shelf") return IIRSubType::HighShelf;
    return IIRSubType::Peak;
}

var EqBand::toJson() const
{
    auto* obj = new DynamicObject();

    if (diagonal_)
        obj->setProperty("diagonal", true);
    else
    {
        obj->setProperty("input_channel", inputChannel_);
        obj->setProperty("output_channel", outputChannel_);
    }

    if (type_ == EqBandType::IIR && hasRawCoeffs_)
    {
        auto* coeffObj = new DynamicObject();
        coeffObj->setProperty("b0", rawCoeffs_.b0);
        coeffObj->setProperty("b1", rawCoeffs_.b1);
        coeffObj->setProperty("b2", rawCoeffs_.b2);
        coeffObj->setProperty("a0", rawCoeffs_.a0);
        coeffObj->setProperty("a1", rawCoeffs_.a1);
        coeffObj->setProperty("a2", rawCoeffs_.a2);
        obj->setProperty("coefficients", var(coeffObj));
    }
    else
    {
        auto* params = new DynamicObject();
        switch (type_)
        {
            case EqBandType::IIR:
                params->setProperty("type", iirSubTypeToString(iirSubType_));
                params->setProperty("f_Hz", frequency_);
                params->setProperty("Q", q_);
                params->setProperty("gain_db", gainDB_);
                break;
            case EqBandType::FIR:
            {
                params->setProperty("type", "fir");
                Array<var> arr;
                for (auto c : firCoeffs_)
                    arr.add(c);
                params->setProperty("coefficients", arr);
                break;
            }
            case EqBandType::Gain:
                params->setProperty("type", "gain");
                params->setProperty("gain_db", gainDB_);
                break;
            case EqBandType::Delay:
                params->setProperty("type", "delay");
                params->setProperty("delay_samples", delaySamples_);
                break;
        }
        obj->setProperty("parameters", var(params));
    }

    if (!enabled_)
        obj->setProperty("enabled", false);

    return var(obj);
}

EqBand* EqBand::fromJson(const var& json)
{
    auto* band = new EqBand();

    // Channel routing
    if (json.hasProperty("diagonal") && (bool)json["diagonal"])
    {
        band->setDiagonal(true);
    }
    else
    {
        band->setDiagonal(false);
        if (json.hasProperty("input_channel"))
            band->setInputChannel((int)json["input_channel"]);
        if (json.hasProperty("output_channel"))
            band->setOutputChannel((int)json["output_channel"]);
    }

    // Raw biquad coefficients
    if (json.hasProperty("coefficients"))
    {
        auto coeffs = json["coefficients"];
        band->setType(EqBandType::IIR);
        band->setRawCoefficients(
            (float)coeffs.getProperty("b0", 1.0),
            (float)coeffs.getProperty("b1", 0.0),
            (float)coeffs.getProperty("b2", 0.0),
            (float)coeffs.getProperty("a0", 1.0),
            (float)coeffs.getProperty("a1", 0.0),
            (float)coeffs.getProperty("a2", 0.0)
        );
    }
    // Parameterized
    else if (json.hasProperty("parameters"))
    {
        auto params = json["parameters"];
        String type = params.getProperty("type", "peak").toString();

        if (type == "gain")
        {
            band->setType(EqBandType::Gain);
            band->setGainDB((float)params.getProperty("gain_db", 0.0));
        }
        else if (type == "delay")
        {
            band->setType(EqBandType::Delay);
            band->setDelaySamples((int)params.getProperty("delay_samples", 0));
        }
        else if (type == "fir")
        {
            band->setType(EqBandType::FIR);
            auto arr = params["coefficients"];
            if (arr.isArray())
            {
                std::vector<float> coeffs;
                for (int i = 0; i < arr.size(); ++i)
                    coeffs.push_back((float)arr[i]);
                band->setFIRCoefficients(coeffs);
            }
        }
        else
        {
            // IIR parameterized
            band->setType(EqBandType::IIR);
            band->setIIRSubType(stringToIIRSubType(type));
            band->setFrequency((float)params.getProperty("f_Hz", 1000.0));
            band->setQ((float)params.getProperty("Q", 0.707));
            band->setGainDB((float)params.getProperty("gain_db", 0.0));
        }
    }

    if (json.hasProperty("enabled"))
        band->setEnabled((bool)json["enabled"]);

    return band;
}
