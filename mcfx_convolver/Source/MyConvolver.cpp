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

#include "MyConvolver.h"

#ifdef USE_ZITA_CONVOLVER

#if ZITA_CONVOLVER_MAJOR_VERSION != 3
#error "This version of IR requires zita-convolver 3.x.x"
#endif

#define CONVPROC_SCHEDULER_PRIORITY 0
#define CONVPROC_SCHEDULER_CLASS SCHED_FIFO
#define THREAD_SYNC_MODE true

#endif

MyConvolver::MyConvolver() : ir_loaded(false),

#ifndef USE_ZITA_CONVOLVER
                    out_buffer(1,512),
#endif
                    bufconv_pos(0),
                    ir_length(0),
                    block_length(512)

{
    
}

MyConvolver::~MyConvolver()
{
#ifdef USE_ZITA_CONVOLVER
    if (ir_loaded)
    {
        conv.stop_process();
    }
    conv.cleanup();
#endif
}

void MyConvolver::unloadIr()
{
#ifdef USE_ZITA_CONVOLVER
    conv.stop_process();
    conv.cleanup();
    
#else
    conv_win.reset();
#endif
}

bool MyConvolver::loadIr(const File& audioFile, int channel, double sampleRate, int BufferLength, float gain, int delay, int offset, int length)
{
    if (!audioFile.existsAsFile())
    {
        std::cout << "ERROR: file does not exist!!" << std::endl;
        return false;
    }
    
    block_length = BufferLength;
    
    AudioFormatManager formatManager;
    
    // this can read .wav and .aiff
    formatManager.registerBasicFormats();
    
    AudioFormatReader* reader = formatManager.createReaderFor(audioFile);
    
    if (!reader) {
        std::cout << "ERROR: could not read impulse response file!" << std::endl;
    }
    
	//AudioFormatReader* reader = wavFormat.createMemoryMappedReader(audioFile);
    
    ir_length = (int)reader->lengthInSamples-offset;
    
    if (ir_length <= 0) {
        std::cout << "wav file has zero samples" << std::endl;
        return false;
    }
    
    if (reader->numChannels <= channel) {
        std::cout << "wav file has too less channels: " << reader->numChannels << std::endl;
        return false;
    }
    
    
    AudioSampleBuffer ReadBuffer(reader->numChannels, ir_length); // create buffer
    
    
    reader->read(&ReadBuffer, 0, ir_length, offset, true, true);
    
    // check if we want a shorter impulse response
    
    if (ir_length > length && length != 0)
        ir_length = length;
    
    // copy the wanted channel into our IR Buffer
    
    AudioSampleBuffer IRBuffer(1, ir_length);
    IRBuffer.copyFrom(0, 0, ReadBuffer, channel, 0, ir_length);
    
    // std::cout << "wav file samplerate: " << reader->sampleRate << " depth: " << reader->bitsPerSample << " numSamples: " << ir_length << " max value: " << HrirBuffer.getMagnitude(0, ir_length) << std::endl;
    
    
    // resample buffer if necessary using soxr
    
    if (sampleRate != reader->sampleRate) {
        // do resampling
        
        double irate = reader->sampleRate;
        double orate = sampleRate;
        
        int newsize = (int) (orate / irate * ir_length + 0.5);
        
        delay = (int) (orate / irate * delay + 0.5);
        // std::cout << "Changing samplerate from " << reader->sampleRate  << " to " << sampleRate << " new length: " << newsize << std::endl;
        
        // allocate resample buffer
        AudioSampleBuffer ResampledBuffer(1, newsize);
        ResampledBuffer.clear();
        
        // do the resampling
        soxr_quality_spec_t const q_spec = soxr_quality_spec(SOXR_HQ, 0);
        size_t odone;
        soxr_error_t error;
        
        error = soxr_oneshot(irate, orate, 1, /* Rates and # of chans. */
                                          IRBuffer.getSampleData(0), ir_length, NULL,         /* Input. */
                                          ResampledBuffer.getSampleData(0), newsize, &odone,    /* Output. */
                                          NULL, // soxr_io_spec_t
                                          &q_spec, // soxr_quality_spec_t
                                          NULL); // soxr_runtime_spec_t
        
        // new length and reset buffer
        ir_length = ResampledBuffer.getNumSamples();
        IRBuffer = ResampledBuffer;
        // std::cout << "Resampled! New HRIR size: " << ir_length << std::endl;
        
    }
    
    // scale ir with gain
    IRBuffer.applyGain(gain);
    
#ifdef USE_ZITA_CONVOLVER
	conv.set_density(0.5f); // should be the fraction of input / output

    int bufsize = block_length; // was uint32_t
    if (bufsize < Convproc::MINPART)
    {
        bufsize = Convproc::MINPART;
    }
    
    int ret = conv.configure( 1, // n_inputs
                              1, // n_outputs
                              ir_length+delay,
                              block_length,
                              bufsize,
                              Convproc::MAXPART);
    if (ret != 0) {
		fprintf(stderr, "IR: can't initialise zita-convolver engine, Convproc::configure returned %d\n", ret);
        // return false;
    }
    

    // send hrir to zita-convolver
    conv.impdata_create(0, 0, 1, IRBuffer.getSampleData(0),
                         delay, ir_length+delay);
    
    conv.start_process(CONVPROC_SCHEDULER_PRIORITY, CONVPROC_SCHEDULER_CLASS);
    
#else
	// copy buffer with delay -> ugly but zita convolver offers more easy solution
	
	AudioSampleBuffer IRBufferDelay(1, ir_length+delay); // create buffer
	IRBufferDelay.clear();
	IRBufferDelay.copyFrom(0,delay,IRBuffer,0,0,ir_length);

	int headBlockSize = block_length;
	int tailBlockSize = std::max(4096, nextPowerOfTwo(ir_length+delay));
	conv_win.init(headBlockSize, tailBlockSize, IRBufferDelay.getSampleData(0), ir_length+delay);
    
#endif
    // delete the audio file reader
    delete reader;
    
    ir_loaded = true;
    return true;
}

double MyConvolver::irLength()
{
    if (ir_loaded) {
        double length = (double)ir_length / sample_rate;
        
        return length;
    } else {
        return 0.f;
    }
        
}

void MyConvolver::process(AudioSampleBuffer& InputBuffer, AudioSampleBuffer& OutputBuffer, int InCh, int OutCh)
{
    if (ir_loaded) {
        
        //conv.print();
        
        int NumSamples = InputBuffer.getNumSamples();
        
#ifdef USE_ZITA_CONVOLVER

        // convolver samples
        float* indata = conv.inpdata(0);
        float* outdata = conv.outdata(0);
        
        
        // processor samples
        float* in_buffer = InputBuffer.getSampleData(InCh);
        
        unsigned int bcp = bufconv_pos;
        
        unsigned int bcp_out = bufconv_pos;
        
        for (int i=0; i < NumSamples; i++) {
            indata[bcp] = (*in_buffer++);
            
            
            if (++bcp == block_length) {
                bcp = 0;
                conv.process(THREAD_SYNC_MODE);
            }
        }
        
        bufconv_pos = bcp;
        
        // copy buffer to output
        OutputBuffer.addFrom(OutCh, 0, outdata+bcp_out, NumSamples);

#else
		out_buffer.setSize(1,NumSamples,false,false,true);
		out_buffer.clear();

		conv_win.process(InputBuffer.getSampleData(InCh), out_buffer.getSampleData(0), NumSamples);

		
        // copy buffer to output
        OutputBuffer.addFrom(OutCh, 0, out_buffer.getSampleData(0), NumSamples);
#endif
        
    }
    
}
