for i in range(16384):
    bfrt.dyso.dyso_pipe_1.Pipe1SwitchIngress.key0.mod(i, f1=7777777)
    bfrt.dyso.dyso_pipe_1.Pipe1SwitchIngress.key1.mod(i, f1=7777777)
    bfrt.dyso.dyso_pipe_1.Pipe1SwitchIngress.key2.mod(i, f1=7777777)
    bfrt.dyso.dyso_pipe_1.Pipe1SwitchIngress.key3.mod(i, f1=7777777)

# Note that 7777777 is the signal of "empty" value