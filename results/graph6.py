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

# x = l1 assoc, y = vc size
def vc_replace_set_assoc(x, y):
    if x == 1 and y == 0:
        return 'Direct Mapped L1 w/ No VC'
    elif x == 1 and y == 2:
        return 'Direct Mapped L1 w/ 2-entry VC'
    elif x == 1 and y == 4:
        return 'Direct Mapped L1 w/ 4-entry VC'
    elif x == 1 and y == 8:
        return 'Direct Mapped L1 w/ 8-entry VC'
    elif x == 1 and y == 16:
        return 'Direct Mapped L1 w/ 16-entry VC'
    elif x == 2:
        return '2-way Set Assoc. L1 w/ No VC'
    elif x == 4:
        return '4-way Set Assoc. L1 w/ No VC'


def xform_set_assoc(x):
    if x > 8:
        return -1
    else:
        return x


def xform_l2s(x):
    if x == 32768:
        return '32KiB'
    elif x == 65536:
        return '64KiB'
    elif x == 131072:
        return '128KiB'
    elif x == 262144:
        return '256KiB'
    elif x == 524288:
        return '512KiB'
    else:
        return '1MiB'


def avg_access_time(ht, l1mr, l1mp):
    total = ht + (l1mr * l1mp)
    return total


def l1l2avg_access_time(l1ht, l1mr, l1mp, l2ht, l2mr, l2mp):
    total = l1ht + (l1mr * (l2ht + (l2mr * l2mp)))
    return total


def l1l2vcavg_access_time(l1ht, l1mr, l1mp, l2ht, l2mr, l2mp, vcat, srr):
    total = l1ht + (srr*vcat) + (l1mr * (l2ht + (l2mr * l2mp)))
    return total


data = pd.read_csv(r'./graph6.csv')
cacti = pd.read_csv(r'./cacti-spreadsheet.csv')
victim = pd.read_csv(r'./cacti-victim.csv')

df = pd.DataFrame(data, columns=[' l1a', ' l1s', ' l1missrate', ' l1read', ' l1write', ' l1readmiss', ' l1writemiss',
                                 ' l2a', ' l2s', ' l2missrate', ' l2read', ' l2write', ' l2readmiss', ' l2writemiss',
                                 ' vcs', ' swapreqs', ' swapreqrate', ' swaps'])
pd.set_option("display.max_rows", None, "display.max_columns", None)
df['log2l1s'] = np.log2(df[' l1s'])

df[' l1a'] = df[' l1a'].apply(xform_set_assoc)
df[' l2a'] = df[' l2a'].apply(xform_set_assoc)

df['l1at'] = np.nan
df['l2at'] = np.nan
df['aat'] = np.nan
df['area'] = np.nan
df['vcat'] = np.nan

for i, row in df.iterrows():
    l1temp = cacti[(row[' l1s'] == cacti['size']) & (cacti['assoc'] == row[' l1a']) & (cacti['blocksize'] == 32)]
    l2temp = cacti[(row[' l2s'] == cacti['size']) & (cacti['assoc'] == row[' l2a']) & (cacti['blocksize'] == 32)]
    vctemp = victim[(row[' vcs'] == victim['vcs'])]

    if not l1temp.empty:
        df['l1at'][i] = l1temp['at']
        df['area'][i] = l1temp['area']

    if not l2temp.empty:
        df['l2at'][i] = l2temp['at']
        df['area'][i] += l2temp['area']

    if vctemp.empty:
        df['vcat'][i] = 0
    else:
        df['vcat'][i] = vctemp['at']
        df['area'][i] += vctemp['area']

# print(df)
#df[' l1a'] = df[' l1a'].apply(vc_replace_set_assoc)
mp = 20.1
for i, row in df.iterrows():
    df['aat'][i] = l1l2vcavg_access_time(row['l1at'], row[' l1missrate'], mp, row['l2at'], row[' l2missrate'], mp, row['vcat'], row[' swapreqrate'])
    df[' l1a'][i] = vc_replace_set_assoc(row[' l1a'], row[' vcs'])
print(df)

#df[' l2s'] = df[' l2s'].apply(xform_l2s)

df.to_csv(r'g6_prepivot_raw.csv')
df = df.pivot(index= 'log2l1s', columns=' l1a', values='aat')
#df.dropna( inplace=True)

print(df)
# y=['Direct Mapped', '2-way Set Assoc.', '4-way Set Assoc.', '8-way Set Assoc.', 'Fully Assoc.'] marker='o'
df.plot(y=['Direct Mapped L1 w/ No VC', 'Direct Mapped L1 w/ 2-entry VC', 'Direct Mapped L1 w/ 4-entry VC',
           'Direct Mapped L1 w/ 8-entry VC', 'Direct Mapped L1 w/ 16-entry VC', '2-way Set Assoc. L1 w/ No VC',
           '4-way Set Assoc. L1 w/ No VC'], marker='x', markersize='5')
fig = plt.gcf()
fig.set_size_inches(8, 6, forward=True)

plt.xlabel("log2(L1_SIZE (B))")
plt.ylabel("Avg. Access Time (nS)")
plt.title("Graph 6: Avg. Access Across Varying L1/L2 Sizes")
plt.legend(title="L2 Size")

outFname = "Graph6-rev.png"
plt.savefig(outFname)
df.to_csv(r'g6_raw.csv')
plt.show()
