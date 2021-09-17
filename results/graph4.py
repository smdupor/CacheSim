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


def xform_l1s(x):
    if x == 1024:
        return '1KiB'
    elif x == 2048:
        return '2KiB'
    elif x == 4096:
        return '4KiB'
    elif x == 8192:
        return '8KiB'
    elif x == 16384:
        return '16KiB'
    else:
        return '32KiB'


def avg_access_time(ht, l1mr, l1mp):
    total = ht + (l1mr*l1mp)
    return total

def l1l2avg_access_time(l1ht, l1mr, l1mp, l2ht, l2mr, l2mp):
    total = l1ht + (l1mr*(l2ht + (l2mr*l2mp)))
    return total


data = pd.read_csv(r'./graph4.csv')
cacti = pd.read_csv(r'./cacti-spreadsheet.csv')
df = pd.DataFrame(data, columns=[' l1a', ' l1s', ' l1missrate', ' l1read', ' l1write', ' l1readmiss', ' l1writemiss',
                                 'bs'])
pd.set_option("display.max_rows", None, "display.max_columns", None)
df['log2bs'] = np.log2(df['bs'])

df[' l1a'] = df[' l1a'].apply(xform_set_assoc)


# df['l1at'] = np.nan
# df['l2at'] = np.nan
# df['aat'] = np.nan
#
# for i, row in df.iterrows():
#     l1temp = cacti[(row[' l1s'] == cacti['size']) & (cacti['assoc'] == row[' l1a']) & (cacti['blocksize'] == 32)]
#     l2temp = cacti[(row[' l2s'] == cacti['size']) & (cacti['assoc'] == row[' l2a']) & (cacti['blocksize'] == 32)]
#     if not l1temp.empty:
#         df['l1at'][i] = l1temp['at']
#
#     if not l2temp.empty:
#         df['l2at'][i] = l2temp['at']

#print(df)
#df[' l1a'] = df[' l1a'].apply(replace_set_assoc)
df[' l1s'] = df[' l1s'].apply(xform_l1s)

# mp = 20.1
# for i, row in df.iterrows():
#     df['aat'][i] = l1l2avg_access_time(row['l1at'], row[' l1missrate'], mp, row['l2at'], row[' l2missrate'], mp)
print(df)

df.to_csv(r'g4_prepivot_raw.csv')
df = df.pivot(index='log2bs', columns=' l1s', values=' l1missrate')


df.plot(y=['1KiB', '2KiB', '4KiB', '8KiB', '16KiB', '32KiB'])


plt.xlabel("log2(BLOCKSIZE (B))")
plt.ylabel("L1 Miss Rate")
plt.title("Graph 4: Miss Rate vs Block Size")
plt.legend(title='L1 Size')

outFname = "Graph4.png"
plt.savefig(outFname)
df.to_csv(r'g4_raw.csv')
plt.show()



