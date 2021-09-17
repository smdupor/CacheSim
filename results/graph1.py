import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl


def replace_set_assoc(x):
    if x == 1:
        return 'Direct Mapped'
    elif x == 2:
        return '2-way Set Assoc.'
    elif x == 4:
        return '4-way Set Assoc.'
    elif x == 8:
        return '8-way Set Assoc.'
    else:
        return 'Fully Assoc.'


data = pd.read_csv(r'./graph1.csv')
df = pd.DataFrame(data, columns=[' l1a', ' l1s', ' l1missrate'])

df['log2l1s'] = np.log2(df[' l1s'])
df[' l1a'] = df[' l1a'].apply(replace_set_assoc)

df = df.pivot(index='log2l1s', columns=' l1a', values=' l1missrate')

print(df)
#render = df.plot(figsize=(5,3))
df.plot( y=['Direct Mapped', '2-way Set Assoc.', '4-way Set Assoc.', '8-way Set Assoc.', 'Fully Assoc.'])

#print(np.mean(dfmult, axis=1))
plt.xlabel("log2(L1_SIZE)")
plt.ylabel("L1 Miss Rate")
plt.title("Graph 1: L1 Miss Rate vs L1 Size(log2)")
plt.legend()
plt.show()
#outFname = sys.argv[1]+".png"
outFname = "Graph1.png"
plt.savefig(outFname)
