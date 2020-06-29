# Kill gdbserver if it's running
ssh larsb@129.242.19.143 killall gdbserver &> /dev/null
# Compile myprogram and launch gdbserver, listening on port 9091
ssh \
  -L9091:localhost:9091 \
  larsb@129.242.19.143 \
  "zsh -l -c 'cd code/diggi && test && gdbserver :9091 ./runtest'"