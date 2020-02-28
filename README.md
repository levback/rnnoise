# RNNoise is a noise suppression library based on a recurrent neural network


## Quick Demo application
While it is meant to be used as a library, a simple command-line tool is
provided as an example.

### build librnnoise & rnnoise_demo with CMake

```shell
# mkdir build
# cd build
# cmake ..
# make
```

It operates on wav and mp3 files, which can be used as:
```shell
# ./rnnoise_demo input.wav
# ./rnnoise_demo input.mp3
```

the output filename is "input_out.wav"
or:

specify the output filename
```shell
# ./rnnoise_demo input.wav output.wav
# ./rnnoise_demo input.mp3 output.wav
```

## Training Process

### Audio feature extract
Build audio feature extraction tool
```shell
# cd src
# ./train_compile.sh
```
Use generated "denoise_training" to get the audio feature array from speech & noise audio clip
```shell
# ./denoise_training
usage: ./denoise_training <speech> <noise> <sample count> <output denoised>
# ./denoise_training speech.wav noise.wav 50000 feature.dat
matrix size: 50000 x 87
```

### RNN model traning
Pick feature array to "training" dir and go through the training process
```shell
# cd training
# mv ../src/feature.dat .
# python bin2hdf5.py --bin_file feature.dat --matrix_shape 50000x87
# python rnn_train.py
# python dump_rnn.py
```
Training process will generate the RNN model weight code file (default is rnn_data.c) and layer definition header file (default is rnn_data.h). They can be used to refresh the "src/rnn_data.c", "src/rnn_data.h" and rebuild the rnnoise lib & demo app.

# References and Resources:
- [david8862/rnnoise](https://github.com/david8862/rnnoise)
- [RNNoise: Learning Noise Suppression](https://people.xiph.org/~jm/demo/rnnoise/)
- [cpuimage/rnnoise](https://github.com/cpuimage/rnnoise)
