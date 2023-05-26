/*
 ==============================================================================

 Copyright (c) 2013/2015 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#ifndef __mcfx_convolver__MtxConv__
#define __mcfx_convolver__MtxConv__

#include "JuceHeader.h"

#if JUCE_MAC
    #define VIMAGE_H // avoid namespace clashes
    #include <Accelerate/Accelerate.h>
    #include <vector>

    #define SPLIT_COMPLEX 1
#else
    #include <fftw3.h>
    const int fftwopt = FFTW_MEASURE; // FFTW_ESTIMATE || FFTW_MEASURE

    #define SPLIT_COMPLEX 0
#endif


#if JUCE_USE_SSE_INTRINSICS
    #if JUCE_LINUX
        #include <x86intrin.h>
    #else
        #include <xmmintrin.h>
    #endif
#endif

#define GARDNER_SCHEME 1

/// Aligned Memory Allocation and Deallocation
inline void* aligned_malloc(size_t size, size_t align) {
    void *result;
#ifdef _MSC_VER
    result = _aligned_malloc(size, align);
#else
    if(posix_memalign(&result, align, size)) result = 0;
#endif
    return result;
}

inline void aligned_free(void *ptr) {
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif

}


////////////////////////////
/** Class which holds frequency domain data of input signal

    Frequency Domain Delay Line (FDL)
 */
class InNode
{
public:
    /// Allocates memory
    InNode(int in, int numpartitions, int partitionsize);

    /// Deallocates memory
    ~InNode();

private:

    friend class MtxConvSlave;

    /// Input channel assignment
    int                 in_;

    /// Number of partitions (length of FDL)
    int                 numpartitions_;

    /// Complex fft data
#if SPLIT_COMPLEX
    float               **a_re_; // N/2+1 -> (M/2+1 = N+1, with M length of fft sequence)
    float               **a_im_; // N/2+1 -> (M/2+1 = N+1)
#else
    fftwf_complex       **a_c_; // N+1 -> interleaved complex data (M+1 with M length of fft)
#endif

};

////////////////////////////
/** Class which holds frequency domain data of filter (x numpartitions_)

    Frequency Domain Delay Line (FDL)
 */
class FilterNode
{
public:
    /// Allocates memory
    FilterNode(InNode *innode, int numpartitions, int partitionsize);

    /// Deallocates memory
    ~FilterNode();

private:

    friend class MtxConvSlave;

    /// Assigned input node
    InNode              *innode_;

    /// Number of partitions (length of FDL)
    int numpartitions_;

    /// Complex fft data
#if SPLIT_COMPLEX
    float               **b_re_; // N/2+1
    float               **b_im_; // N/2+1
#else
    fftwf_complex       **b_c_; // N+1 -> interleaved complex data
#endif

};

////////////////////////////
// class which holds time domain output data
// and the frequency domain output for each partition
class OutNode
{
public:
    /// Allocates memory
    OutNode(int out, int partitionsize, int numpartitions);

    /// Dellocates memory
    ~OutNode();

private:

    friend class MtxConvSlave;

    /// Output channel assignement
    int                 out_;

    /// A list of all assigned filternodes
    Array<FilterNode*>  filternodes_;

    /// Output samples, 1 channels (no concur. access problem)
    AudioSampleBuffer   outbuf_;

    int                 numpartitions_;

    /// complex fft data
#if SPLIT_COMPLEX
    float               **c_re_; // N/2+1
    float               **c_im_; // N/2+1
#else
    fftwf_complex       **c_c_; // N+1 -> interleaved complex data
#endif
};


///////////////////////////
// this is the class which does the actual processing
class MtxConvSlave : public Thread
{
public:
    MtxConvSlave ();

    ~MtxConvSlave ();

private:
    friend class MtxConvMaster;

    /// threadfunct
    void run ();

    /// this is called by the callback or thread to perform convolution of one input signal partition
    void Process (int filt_part_idx);

    /// transform the new input data
    void TransformInput(bool skip);

    /// transform the accumulated output
    void TransformOutput(bool skip);

    /// Write the time data to the master output buffer
    void WriteToOutbuf(int numsamples, bool skip);

    /** This is called to get the time domain output signal

        returns true if all partitions finished in time, false if some have been skipped
        if forcesync = true this will wait until all partitions are finished
     */
    bool ReadOutput(int numsamples, bool forcesync);

    /// start/stop thread
    void StartProc();
    void StopProc();

    bool Configure ( int partitionsize,
                     int numpartitions,
                     int offset,
                     int priority,
                     AudioSampleBuffer *inbuf,
                     AudioSampleBuffer *outbuf );

    void SetBufsize ( int inbufsize, int outbufsize, int blocksize );

    bool AddFilter ( int in,
                     int out,
                     const AudioSampleBuffer& data);

    /// Return InNode ID if found,  otherwise returns id of newly created or -1 if not available
    int CheckInNode (int in, bool create);

    /// Return OutNode ID if found, otherwise returns id of newly created or -1 if not available
    int CheckOutNode (int out, bool create); //

    /// Set all Input/Output Buffers to Zero
    void Reset ();

    /// Destroy all allocated Memory....
    void Cleanup ();

    /// print debug info
    void DebugInfo();

	/// Write to debug file
	void WriteLog(String &text);

    /// Shared Input Buffer
    AudioSampleBuffer   *inbuf_;

    /// Shared Output Buffer
    AudioSampleBuffer   *outbuf_;


    /// Size of time domain input buffer (2*maxpart_)
	int					inbufsize_;

    /// Size of time domain output buffer (2*maxsize_)
	int                 outbufsize_;

    /// Current input ring buffer offset
    int                 inoffset_;

    /// Current output ring buffer offset (of shared output buf)
    int                 outoffset_;

    /// Number of new samples in the input buffer since last process
    int                 numnewinsamples_;

    /// Offset in the outnode buffer
    int                 outnodeoffset_;

    /// Partition index for Frequency Domain Delay Line
    int                 part_idx_;

    /// Counter how many partitions are done
    Atomic<int>         finished_part_;

    /// Counter of how many cycles should be skipped on next processing cycle...
    Atomic<int>         skip_cycles_;

    /// Number of partitions within this level (-> with same size)
    int                 numpartitions_;

    /// Size of the partition (fft will be 2x this size!)
    int                 partitionsize_;

    /// Offset in the impulse response for the first partition of this size
    int                 offset_;

    /// thread priority... short partitions have to deliver first!
    int                 priority_;

    /// this will signal the threads that new work is here!
    WaitableEvent       waitnewdata_;

    /// this will signal thread work is done.
    WaitableEvent       waitprocessing_;

    /** time data for FFT / iFFT

        2*N
        M = 2*N, inNode will split complex numbers in M/2
     */
    float               *fft_t_;

    /// Normalization for fft
    float               fft_norm_;

#if SPLIT_COMPLEX
    /// Apple vDSP FFT plan
    FFTSetup            vdsp_fft_setup_;
    DSPSplitComplex     splitcomplex_;

    /// N+1
    float               *fft_re_;
    /// N+1
    float               *fft_im_;
    /// vDSP needs the exponent of two
    int                 vdsp_log2_;
#else
    /// FFTWF forward plan
    fftwf_plan          fftwf_plan_r2c_;

    /// FFTWF inverse plan
    fftwf_plan          fftwf_plan_c2r_;

    /// FFTWF buffer for time domain signal (2*N)
    float               *fftwf_t_data_;

    /// FFTWF buffer for complex signal (N+1)
    fftwf_complex        *fft_c_;
#endif

    /// Holds input nodes
    OwnedArray<InNode>      innodes_;

    /// Holds filter nodes
    OwnedArray<FilterNode>  filternodes_;

    /// Holds output nodes
    OwnedArray<OutNode>     outnodes_;

    /// Debug output Text File
	std::unique_ptr<FileOutputStream>   debug_out_;

#if GARDNER_SCHEME
    WaitableEvent       inter_sync;
    int                 inter_offset;
#endif

};


///////////////////////////
// this is the class which manages the processing

class MtxConvMaster
{


public:

    friend class MtxConvSlave;


    MtxConvMaster();
    ~MtxConvMaster();


    // Configure the Convolution Machine with fixed In/Out Configuration,
    // Blocksize and Max Impulse Response length
    bool Configure ( int numins,
                     int numouts,
                     int blocksize,
                     int maxsize,
                     int minpart,
                     int maxpart,
                     bool safemode=false); // will add a delay of minpart_ samples to make sure there is never a buffer underrun for hosts that dare to send partial blocks(Adobe and Steinberg)

    // Add an Impulse Response with dedicated Input/Output assignement
    bool AddFilter ( int in,
                     int out,
                     const AudioSampleBuffer& data );

    // Start the Processing
    void StartProc();

    // Stop the Processing
    void StopProc();

    // Do the processing
    // forcesync = true will make sure that all partitions are going to be finished - use this for offline rendering!
    void processBlock(AudioSampleBuffer& inbuf, juce::AudioSampleBuffer &outbuf, int numsamples, bool forcesync=true);


    // Set all Input/Output Buffers to Zero
    void Reset ();


    // Remove all Filter and Set Buffers to zero
    void Cleanup ();

    // print debug info
    void DebugInfo();

	// write to debug file
	void WriteLog(String &text);

    // get the number of skipped cycles due to unfinished partitions
    int getSkipCount()
    {
        return skip_count_;
    }

    int getMaxSize()
    {
        return maxsize_;
    }

private:

    AudioSampleBuffer   inbuf_;             // Holds the Time Domain Input Samples
    AudioSampleBuffer   outbuf_;            // Hold the Time Domain Output Samples

    int                 inbufsize_;         // size of time domain input buffer (2*maxpart_)
    int                 outbufsize_;        // size of time domain output buffer (2*maxsize_)

    int                 inoffset_;          // current ring buffer write offset
    int                 outoffset_;         // current ring buffer read offset

    int                 blocksize_;         // Blocksize of host process (how many samples are processed in each callback)

    int                 minpart_;           // Size of first partition
    int                 maxpart_;           // Maximum partition size

    int                 numins_;            // Number of Input Channels
    int                 numouts_;           // Number of Output Channels

    int                 numpartitions_;     // Number of Partition Levels (different blocklengths)

    int                 skip_count_;        // The number of skipped partitions

    int                 maxsize_;           // maximum filter length

    bool                configuration_;     // isconfigured

    CriticalSection     lock_;              // lock critical section

    OwnedArray<MtxConvSlave>    partitions_;// these are my partitions with different size

    std::unique_ptr<FileOutputStream>   debug_out_; // Debug output Text File
};


#endif /* defined(__mcfx_convolver__MtxConv__) */
