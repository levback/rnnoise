# RNNoise is a noise suppression library based on a recurrent neural network
 - ref: https://github.com/xiph/rnnoise

To compile, just use CMake.

While it is meant to be used as a library, a simple command-line tool is
provided as an example. 

It operates on wav and mp3 files. It can be used as:

./rnnoise_demo input.wav

./rnnoise_demo input.mp3

the output filename is "input_out.wav"

or:

specify the output filename

./rnnoise_demo input.wav output.wav

./rnnoise_demo input.mp3 output.wav


# Donating

If you found this project useful, consider buying me a coffee

<a href="https://www.buymeacoffee.com/gaozhihan" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/black_img.png" alt="Buy Me A Coffee" style="height: auto !important;width: auto !important;" ></a>
