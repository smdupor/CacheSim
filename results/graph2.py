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


def xform_set_assoc(x):
    if x > 8:
        return -1
    else:
        return x


def avg_access_time(ht, l1mr, l1mp):
    total = ht + (l1mr*l1mp)
    return total



data = pd.read_csv(r'./graph1.csv')
cacti = pd.read_csv(r'./cacti-spreadsheet.csv')
df = pd.DataFrame(data, columns=[' l1a', ' l1s', ' l1missrate', ' l1read', ' l1write', ' l1readmiss', ' l1writemiss'])
pd.set_option("display.max_rows", None, "display.max_columns", None)
df['log2l1s'] = np.log2(df[' l1s'])

df[' l1a'] = df[' l1a'].apply(xform_set_assoc)

df['at'] = np.nan
df['aat'] = np.nan

for i, row in df.iterrows():
    temp = cacti[(row[' l1s'] == cacti['size']) & (cacti['assoc'] == row[' l1a']) & (cacti['blocksize'] == 32)]
    if not temp.empty:
        df['at'][i] = temp['at']

#print(df)
df[' l1a'] = df[' l1a'].apply(replace_set_assoc)
mp = 20.1
for i, row in df.iterrows():
    df['aat'][i] = avg_access_time(row['at'], row[' l1missrate'], mp)
print(df)

df = df.pivot(index='log2l1s', columns=' l1a', values='aat')


df.plot(y=['Direct Mapped', '2-way Set Assoc.', '4-way Set Assoc.', '8-way Set Assoc.', 'Fully Assoc.'])


plt.xlabel("log2(L1_SIZE (B))")
plt.ylabel("Avg. Access Time (nS)")
plt.title("Graph 2: Avg. Access Time vs L1 Size(log2)")
plt.legend()
plt.show()


outFname = "Graph2.png"
plt.savefig(outFname)

