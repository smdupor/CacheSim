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

def l1l2avg_access_time(l1ht, l1mr, l1mp, l2ht, l2mr, l2mp):
    total = l1ht + (l1mr*(l2ht + (l2mr*l2mp)))
    return total


data = pd.read_csv(r'./graph3.csv')
cacti = pd.read_csv(r'./cacti-spreadsheet.csv')
df = pd.DataFrame(data, columns=[' l1a', ' l1s', ' l1missrate', ' l1read', ' l1write', ' l1readmiss', ' l1writemiss',
                                 ' l2a', ' l2s', ' l2missrate', ' l2read', ' l2write', ' l2readmiss', ' l2writemiss'])
pd.set_option("display.max_rows", None, "display.max_columns", None)
df['log2l1s'] = np.log2(df[' l1s'])

df[' l1a'] = df[' l1a'].apply(xform_set_assoc)
df[' l2a'] = df[' l2a'].apply(xform_set_assoc)

df['l1at'] = np.nan
df['l2at'] = np.nan
df['aat'] = np.nan

for i, row in df.iterrows():
    l1temp = cacti[(row[' l1s'] == cacti['size']) & (cacti['assoc'] == row[' l1a']) & (cacti['blocksize'] == 32)]
    l2temp = cacti[(row[' l2s'] == cacti['size']) & (cacti['assoc'] == row[' l2a']) & (cacti['blocksize'] == 32)]
    if not l1temp.empty:
        df['l1at'][i] = l1temp['at']

    if not l2temp.empty:
        df['l2at'][i] = l2temp['at']

#print(df)
df[' l1a'] = df[' l1a'].apply(replace_set_assoc)
mp = 20.1
for i, row in df.iterrows():
    df['aat'][i] = l1l2avg_access_time(row['l1at'], row[' l1missrate'], mp, row['l2at'], row[' l2missrate'], mp)
print(df)

df.to_csv(r'g3_prepivot_raw.csv')
df = df.pivot(index='log2l1s', columns=' l1a', values='aat')


df.plot(y=['Direct Mapped', '2-way Set Assoc.', '4-way Set Assoc.', '8-way Set Assoc.', 'Fully Assoc.'])


plt.xlabel("log2(L1_SIZE (B))")
plt.ylabel("Avg. Access Time (nS)")
plt.title("Graph 3: Avg. Access Time With L2 Cache Enabled")
plt.legend()

outFname = "Graph3.png"
plt.savefig(outFname)
df.to_csv(r'g3_raw.csv')
plt.show()



