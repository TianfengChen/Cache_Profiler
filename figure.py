import numpy as np
import matplotlib.pyplot as plt

mode = "dyn_10_10"

y = np.loadtxt(mode + ".out")
ipc = y[:, 0]
br = y[:, 1]
c = y[:, 2]
x = y[:, 3]

n = y.shape[0]
for i in range(1,n):
    x[i] = x[i] + x[i-1]



plt.figure(figsize=(20,5))

plt.plot(x, c)
plt.savefig("cache_miss_" + mode)
plt.clf()
plt.plot(x, br)
plt.savefig("br_miss_" + mode)
plt.clf()
plt.plot(x, ipc)
plt.savefig("ipc_" + mode)
