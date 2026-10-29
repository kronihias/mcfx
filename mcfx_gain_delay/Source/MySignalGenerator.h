/*
 ==============================================================================
 
 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com
 
 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)
 
 Details of these licenses can be found at: www.gnu.org/licenses
 
 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 
 ==============================================================================
 */

#ifndef __mcfx_gain_delay__MySignalGenerator__
#define __mcfx_gain_delay__MySignalGenerator__

#include <JuceHeader.h>

#define _USE_MATH_DEFINES
#include <math.h>

/////////////////////////////////

/* RANDOM GENERATOR from Numerical Recipes in C */

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

class MyRandomGenerator
{
public:
  MyRandomGenerator()
  {
    iy = 0;
    setSeed(-(long)(Random::getSystemRandom().nextInt(IM - 1) + 1)); // own seed per instance
    
    iset=false;
  }
  
  ~MyRandomGenerator()
  {
    
  }
  
  
  /* initialize first with negative integer (long) */
  void setSeed(long idum_)
  {
    idum = idum_;
    
    int j;
    long k;
    
    if (idum <= 0 || !iy) {
      if (-(idum) < 1)
        idum=1;
      else
        idum = -(idum);
      for (j=NTAB+7;j>=0;j--)
      {
        k=idum/IQ;
        idum=IA*(idum-k*IQ)-IR*k;
        if (idum < 0) idum += IM;
        if (j < NTAB) iv[j] = idum;
      }
      
      iy=iv[0];
    }
  }
  
  /* Random number between -1.0 and 1.0 */
  float getRandomNumber()
  {
    float temp;
    
    int j;
    
    long k=idum/IQ;
    idum=IA*(idum-k*IQ)-IR*k;
    
    if (idum < 0) idum += IM;
    
    j=iy/NDIV;
    iy=iv[j];
    iv[j] = idum;
    
    temp=AM*iy;
    
    if (temp > RNMX)
      return (2.f * RNMX - 1.f);
    else
      return (2.f * temp - 1.f);
    
  }
  
  /* Get Gaussian distributed Random Number */
  /* zero mean and unit variance */
  float getGaussianRandomNumber()
  {
    
    float fac,rsq,v1,v2;
    
    if (iset == 0) {
      
      do {
        
        v1=getRandomNumber();
        v2=getRandomNumber();
        rsq=v1*v1+v2*v2;
        
      } while (rsq >= 1.0 || rsq == 0.0);
      
      fac=sqrt(-2.0*log(rsq)/rsq)*0.2239f; // -12dB
      gset=v1*fac;
      iset=true;
      
      return v2*fac;
    }
    else
    {
      iset=false;
      return gset;
    }
  }
  
  /* Fill all channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithGaussianRandomNumbers(AudioSampleBuffer &buffer)
  {
    int numSamples = (&buffer)->getNumSamples();
    int numChannels = (&buffer)->getNumChannels();
    
    for (int i=0; i<numChannels; i++) {
      float* wp = (&buffer)->getWritePointer(i);
      
      for (int k=0; k<numSamples; k++) {
        (*wp++) = getGaussianRandomNumber();
      }
    }
    
  }
  
  /* Fill one channel in AudioBuffer with random numbers aka white noise */
  void fillBufferWithGaussianRandomNumbers(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    
    for (int k=0; k<numSamples; k++) {
      (*wp++) = getGaussianRandomNumber();
    }
    
  }
  
  /* Fill all channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithRandomNumbers(AudioSampleBuffer &buffer)
  {
    int numSamples = (&buffer)->getNumSamples();
    int numChannels = (&buffer)->getNumChannels();
    
    for (int i=0; i<numChannels; i++) {
      float* wp = (&buffer)->getWritePointer(i);
      
      for (int k=0; k<numSamples; k++) {
        (*wp++) = getRandomNumber();
      }
    }
    
  }
  
  /* Fill one channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithRandomNumbers(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    
    for (int k=0; k<numSamples; k++) {
      (*wp++) = getRandomNumber();
    }
    
  }
  
private:
  
  long idum;
  long iy;
  long iv[NTAB];
  
  
  // for gaussian
  float gset;
  bool iset;
};


/* Pink Noise Filter */

class MyPinkNoiseFilter
{
public:
  MyPinkNoiseFilter(int numchannels=1)
  {
    numChannels_ = numchannels;
    
    b0_.resize(numChannels_);
    b1_.resize(numChannels_);
    b2_.resize(numChannels_);
    b3_.resize(numChannels_);
    b4_.resize(numChannels_);
    b5_.resize(numChannels_);
    b6_.resize(numChannels_);
    
    ext_x1_.resize(numChannels_);
    ext_y1_.resize(numChannels_);
    
    for (int i=0; i < numChannels_; i++)
    {
      ext_x1_.set(i, 0.f);
      ext_y1_.set(i, 0.f);
      b0_.set(i, 0.f);
      b1_.set(i, 0.f);
      b2_.set(i, 0.f);
      b3_.set(i, 0.f);
      b4_.set(i, 0.f);
      b5_.set(i, 0.f);
      b6_.set(i, 0.f);
    }
  }
  
  ~MyPinkNoiseFilter()
  {
    
  }
  
  // above 96 kHz the filter below would flatten under 40 Hz, extend it with a low shelf
  void setSamplerate(float samplerate)
  {
    ext_on_ = samplerate > 100000.f;
    ext_p_ = expf(-2.f*(float)M_PI*3.3f/samplerate);
  }
  
  // http://www.firstpr.com.au/dsp/pink-noise/
  void FilterAudioBuffer(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    float* b0 = &b0_.getReference(ch);
    float* b1 = &b1_.getReference(ch);
    float* b2 = &b2_.getReference(ch);
    float* b3 = &b3_.getReference(ch);
    float* b4 = &b4_.getReference(ch);
    float* b5 = &b5_.getReference(ch);
    float* b6 = &b6_.getReference(ch);
      
      for (int k=0; k<numSamples; k++) {
        
        float white = *wp;
        
        *b0 = 0.99886 * *b0 + white * 0.0555179;
        *b1 = 0.99332 * *b1 + white * 0.0750759;
        *b2 = 0.96900 * *b2 + white * 0.1538520;
        *b3 = 0.86650 * *b3 + white * 0.3104856;
        *b4 = 0.55000 * *b4 + white * 0.5329522;
        *b5 = -0.7616 * *b5 - white * 0.0168980;
        (*wp++) = (*b0 + *b1 + *b2 + *b3 + *b4 + *b5 + *b6 + white * 0.5362) * 0.2959f;
        *b6 = white * 0.115926;
        
      }
    
    if (ext_on_)
    {
      wp = (&buffer)->getWritePointer(ch);
      float* x1 = &ext_x1_.getReference(ch);
      float* y1 = &ext_y1_.getReference(ch);
      
      for (int k=0; k<numSamples; k++) {
        float x = *wp;
        *y1 = x - 0.999655f * *x1 + ext_p_ * *y1;
        *x1 = x;
        (*wp++) = *y1;
      }
    }

  }
  
  void FilterAudioBuffer(AudioSampleBuffer &buffer)
  {
    int numChannels = jmin((&buffer)->getNumChannels(), numChannels_);
    
    for (int i=0; i<numChannels; i++) {
      FilterAudioBuffer(buffer, i);
    }
  }
  
private:
  Array<float> b0_, b1_, b2_, b3_, b4_, b5_, b6_;
  Array<float> ext_x1_, ext_y1_;
  bool ext_on_ = false;
  float ext_p_ = 0.f;
  int numChannels_;
  
};

/* Tone Generators */

class MyOscillator
{
public:
  void setSamplerate(float samplerate)
  {
    samplerate_ = samplerate > 0.f ? samplerate : 44100.f;
  }
  
  void setFreq(float f)
  {
    f_ = f > 0.f ? f : 440.f;
  }
  
  float getFreq()
  {
    return f_;
  }
  
protected:
  float samplerate_ = 44100.f;
  float f_ = 440.f;
};


/* Sine Generator */
// recursive quadrature oscillator in double precision
class MySineGenerator : public MyOscillator
{
public:
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch)
  {
    if (f_ != coeff_f_ || samplerate_ != coeff_sr_)
    {
      coeff_f_ = f_;
      coeff_sr_ = samplerate_;
      const double w = 2.*M_PI*(double)coeff_f_/(double)coeff_sr_;
      c_ = cos(w);
      s_ = sin(w);
    }
    
    int numSamples = buffer.getNumSamples();
    float *out = buffer.getWritePointer(ch);
    
    double re = re_, im = im_;
    const double c = c_, s = s_;
    
    for (int i=0; i<numSamples; i++)
    {
      *out++ = (float)im;
      
      const double re_new = re*c - im*s;
      im = re*s + im*c;
      re = re_new;
    }
    
    // renormalise magnitude
    const double g = 1.5 - 0.5*(re*re + im*im);
    re_ = re*g;
    im_ = im*g;
  }
  
private:
  float coeff_f_ = 0.f;
  float coeff_sr_ = 0.f;
  
  double re_ = 1., im_ = 0.;
  double c_ = 1., s_ = 0.;
};


/* Sawtooth and Square Generator */
// PolyBLEP band limited
class MyBlepGenerator : public MyOscillator
{
public:
  enum shape { saw, square };
  
  MyBlepGenerator(shape s) : shape_(s) {}
  
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = buffer.getNumSamples();
    float *out = buffer.getWritePointer(ch);
    
    const double dt = jmin(0.5, (double)f_/(double)samplerate_);
    double t = t_;
    
    for (int i=0; i<numSamples; i++)
    {
      double y;
      
      if (shape_ == saw)
      {
        y = 2.*t - 1. - blep(t, dt);
      }
      else
      {
        double t2 = t + 0.5;
        if (t2 >= 1.)
          t2 -= 1.;
        
        y = (t < 0.5 ? 1. : -1.) + blep(t, dt) - blep(t2, dt);
      }
      
      *out++ = (float)(0.707*y);
      
      t += dt;
      if (t >= 1.)
        t -= 1.;
    }
    
    t_ = t;
  }
  
private:
  static double blep(double t, double dt)
  {
    if (t < dt)
    {
      t /= dt;
      return t + t - t*t - 1.;
    }
    if (t > 1. - dt)
    {
      t = (t - 1.)/dt;
      return t*t + t + t + 1.;
    }
    return 0.;
  }
  
  shape shape_;
  double t_ = 0.;
};


/* Toneburst Generator */
// 6.5 cycles with hann window, then 0.6 s pause
class MyToneburstGenerator : public MyOscillator
{
public:
  void setSamplerate(float samplerate)
  {
    MyOscillator::setSamplerate(samplerate);
    pos_ = burst_len_ = pause_len_ = 0;
  }
  
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = buffer.getNumSamples();
    float *out = buffer.getWritePointer(ch);
    
    for (int i=0; i<numSamples; i++)
    {
      if (pos_ >= burst_len_ + pause_len_)
        startBurst();
      
      if (pos_ < burst_len_)
      {
        *out++ = (float)(0.5*(1. - wre_)*im_);
        
        double tmp = re_*c_ - im_*s_;
        im_ = re_*s_ + im_*c_;
        re_ = tmp;
        
        tmp = wre_*wc_ - wim_*ws_;
        wim_ = wre_*ws_ + wim_*wc_;
        wre_ = tmp;
      }
      else
      {
        *out++ = 0.f;
      }
      
      pos_++;
    }
  }
  
private:
  void startBurst()
  {
    const double w = 2.*M_PI*(double)f_/(double)samplerate_;
    c_ = cos(w);
    s_ = sin(w);
    wc_ = cos(w/6.5);
    ws_ = sin(w/6.5);
    re_ = wre_ = 1.;
    im_ = wim_ = 0.;
    
    burst_len_ = (int)ceil(6.5*samplerate_/f_);
    pause_len_ = (int)(0.6f*samplerate_);
    pos_ = 0;
  }
  
  int pos_ = 0, burst_len_ = 0, pause_len_ = 0;
  
  double re_ = 1., im_ = 0., c_ = 1., s_ = 0.; // tone
  double wre_ = 1., wim_ = 0., wc_ = 1., ws_ = 0.; // window
};

class MySignalGenerator
{
public:
  
  MySignalGenerator(float samplerate=44100.f, int numchannels=1);
  ~MySignalGenerator();
  
  /* enum defs */
  enum sigtype { white, pink, sine, sawtooth, square, dirac, toneburst, numSigTypeParams };
  enum sigtime { steady, pulsed, numSigTimeParams };
  
  /* member functs */
  void setSamplerate(float samplerate);
  
  void setGaindB(float db);
  float getGaindB();
  
  void setGain(float mag);
  float getGain();
  
  // normalized parameter 0.0 ... -inf dB;  1.0... +6dB
  void setGainParam(float param);
  float getGainParam();
  String getGainText();
  
  void setSignalType(sigtype signal_type);
  sigtype getSignalType();
  
  void setSignalTypeParam(float param);
  float getSignalTypeParam();
  String getSignalTypeText();
  
  void setSignalTime(sigtime signal_time);
  sigtime getSignalTime();
  
  void setSignalTimeParam(float param);
  float getSignalTimeParam();
  String getSignalTimeText();
  
  void setPulseLength(float seconds);
  float getPulseLength();
  
  void setFreq(float f);
  float getFreq();
  void setFreqParam(float param);
  float getFreqParam();
  String getFreqText();
  
  
  void fillBufferWithSignal(AudioSampleBuffer &buffer);
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch);
  
  
  
  
  
private:
  int numchannels_;
  MyRandomGenerator random_gen_;
  MyPinkNoiseFilter pink_noise_filt_;
  MySineGenerator sine_generator_;
  MyBlepGenerator sawtooth_generator_ { MyBlepGenerator::saw };
  MyBlepGenerator square_generator_ { MyBlepGenerator::square };
  MyToneburstGenerator tuneburst_generator_;

  sigtype signal_type_;
  sigtime signal_time_;
  
  float wantedgain_; // the gain requested
  
  float gainstate_; // current gain state for ramping
  
  float pulse_len_s_; // pulse length in seconds
  int pulse_len_; // length of the pulse/pause in samples
  int ramp_len_; // pulse fade in/out in samples
  int cur_time_smpl_; // used for pulsed signal type
  
  float samplerate_;
  
};

#endif /* defined(__mcfx_gain_delay__MySignalGenerator__) */
