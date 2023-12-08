function write_mcfx_convolver_file(mat, fs, filename)
% write_mcfx_convolver_file(mat, fs, filename)
% 
% write config file and .wav file for a dense convolution matrix loadable
% by mcfx_convolver
%
% mat... matrix containing the impulse responses
% 3: [input x output x samples]     3 dimensions of mat 
% 2: [output x samples] 2 dimensions of mat: one input channel
% 1: [samples]          1 dimension of mat: one input and output channel
%
% fs is SamplingRate
%
% filename is without extension (.wav)
%
% filename.wav and filename.conf will be created for you

% align the impulse responses sequentially if multiple input channels are
% used

% n... num inputs
% m... num outputs
% l... impulse response length
%
%
% 2016 Matthias Kronlachner


if ndims(mat) == 1
    m=1; % out
    n=1; % in
    l=size(mat,1); % samples
    
    x = zeros(n,m,l);
    x(1,1,:) = mat;
    
elseif ndims(mat) == 2
    m=size(mat,1); % out
    n=1; % in
    l=size(mat,2); % samples
    
    x = zeros(n,m,l);
    x(1,:,:) = mat;
    
elseif ndims(mat) == 3
    n=size(mat,1);
    m=size(mat,2);
    l=size(mat,3);
    x=mat;
else
    display('Error: mat has more than 3 dimensions!')
    return;
end

% n... num inputs
% m... num outputs
% l... impulse response length
%
% x has dimensions n, m, l

% sequentially store impulse responses
y = zeros(l*n, m); % wav has to be: l, m

for n_i = 1:n
    y((n_i-1)*l+1:(n_i)*l, :) = squeeze(x(n_i, :, :))';
end

% write multichannel audio file

% include the number of input channels in the comment metadata - this
% allows the .wav file to be loaded without the .conf file!
audiowrite([filename '.wav'], y, fs, 'BitsPerSample', 32, 'Comment', num2str(n));

[a b c] = fileparts(filename);
filename_wav = [b c];

% write the configuration file
fid = fopen([filename '.conf'],'w+');

fprintf(fid,[...
'# mcfx_convolver configuration\n'...
'# ------------------------\n'...
'/cd \n'...
'#\n'...
'#\n'...
'#\n'...
'#               #inchannels  gain  delay  offset  length        filename  \n'...
'# ------------------------------------------------------------------------------\n'...
'#\n'...
'/impulse/packedmatrix ' num2str(n) ' 1.0 0 0 0 ' filename_wav '.wav\n']);

fclose(fid);

