function write_mcfx_diagonal_convolver_file(FilterMatrix, fs, Filename)
% write_mcfx_diagonal_convolver_file(mat, fs, filename)
%
% N Filters for N In/Outs
%
% write a convolver file filtering each of the N input channels 
% with one impulse response, generating N output channels
%
% FilterMatrix Dimensions: M Samples/Taps x N In/Output Channels
%
% Matthias Kronlachner

if ndims(FilterMatrix) > 2
   error("FilterMatrix has to have 1 or 2 dimensions!") 
end

NumChannels = size(FilterMatrix, 2);


[a b c] = fileparts(Filename);
FilenameHeader = [a b];

% write multichannel audio file

% include -1 as comment metadata - this
% allows the .wav file to be loaded without the .conf file!
audiowrite([FilenameHeader '.wav'], FilterMatrix, fs, 'BitsPerSample', 32, 'Comment', '-1');


% write the configuration file
fid = fopen([FilenameHeader '.conf'],'w+');

fprintf(fid,[...
'# mcfx_convolver configuration\n'...
'# ------------------------\n'...
'/cd \n'...
'#\n'...
'#\n'...
'#\n'...
'#               in out  gain  delay  offset  length  chan      file  \n'...
'# ------------------------------------------------------------------------------\n'...
'#\n']);

for i=1:NumChannels
    fprintf(fid, ['/impulse/read ' num2str(i) ' ' num2str(i) ' 1.0 0 0 0 ' num2str(i) ' ' b '.wav\n']);
end

fclose(fid);

