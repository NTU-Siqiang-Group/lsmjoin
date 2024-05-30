import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.lines as mlines
from csv_process import write_csv_from_txt, process_csv
import sci_palettes
from scipy.spatial.distance import pdist, squareform

fontsize = 12
edgewidth = 1.5
markersize = 5
sci_palettes.register_cmap()
test_names = ["workload"]
for test_name in test_names:
    write_csv_from_txt(test_name)
    process_csv(test_name)

workload = pd.read_csv("lsm_join/csv_result/workload.csv")


# 创建数据框的数组
dfs = [
    {"df": workload, "title": "workload"},
]

# 设置图的大小和子图布局
fig, axes = plt.subplots(1, 1, figsize=(4, 3), sharey=True)  # 一行两列

# colors = ["#3E8D27", "#A22025", "#1432F5"]
style = "tab10"
plt.set_cmap(style)
cmap = plt.get_cmap(style)
colors = cmap.colors
darkening_factor = 0.5
colors = [
    (r * darkening_factor, g * darkening_factor, b * darkening_factor)
    for r, g, b in colors
]

label_settings = {
    "CEager-INLJ": {"color": colors[0], "marker": "o"},
    "CLazy-INLJ": {"color": colors[1], "marker": "d"},
    "CComp-INLJ": {"color": colors[2], "marker": "^"},
}

ax = axes
df = dfs[0]["df"]
attribute = "num_loop"
fillstyle = "none"
title = attribute

for label, group in df.groupby("label"):

    color = label_settings[label]["color"]
    marker = label_settings[label]["marker"]

    ax.plot(
        group[attribute],
        group["sum_join_time"] + group["sum_index_build_time"],
        marker=marker,
        fillstyle=fillstyle,
        color=color,
        linewidth=edgewidth,
        markeredgewidth=edgewidth,
        markersize=markersize,
    )

ax.set_ylabel("System Latency (s)", fontweight="bold", fontsize=fontsize)
ax.set_xlabel(title, fontweight="bold", fontsize=fontsize)


legend_handles2 = []
for label, setting in label_settings.items():
    legend_handles2.append(
        mlines.Line2D(
            [],
            [],
            color=setting["color"],
            marker=setting["marker"],
            linestyle="-",
            markersize=markersize,
            fillstyle="none",
            label=label,
        )
    )

fig.legend(
    handles=legend_handles2,
    # bbox_to_anchor=(0.65, 1.1),
    # ncol=7,
    fontsize=fontsize - 1,
    edgecolor="black",
    loc="upper left",
)

# plt.yscale('log')

plt.subplots_adjust(wspace=0.01, hspace=0.1)
plt.tight_layout()
plt.savefig("lsm_join/plot/workload.pdf", bbox_inches="tight", pad_inches=0.02)
plt.close()