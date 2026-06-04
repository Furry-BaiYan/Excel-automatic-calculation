# Excel Processor

一个基于 **C++ Qt6** 开发的桌面端 Excel 数据自动处理软件，支持公式计算、统计汇总、数据清洗，并可导出格式化的 xlsx 和 PDF 报告。内置 AI 分析引擎，能够自动识别数据结构并推荐计算公式。

---

## ✨ 功能特性

### 📂 数据读取
- 自动探测 `.xlsx` 文件结构（表头行、列数、数据类型）
- 自动推断每列类型（文本 / 整数 / 小数 / 日期 / 布尔）
- 支持批量读取整个文件夹

### 🧮 公式计算引擎
直接输入类 Excel 公式，系统**自动识别**运算类型，无需手动选择：

```
总分 = 数学 + 语文 + 英语
平均分 = ROUND(总分 / 3, 1)
等级 = IFS(总分>=270, "优秀", 总分>=225, "良好", "待改进")
税后工资 = 基本工资 * 0.8 + 奖金
净利润率 = (收入 - 成本) / 收入 * 100
```

**支持的函数（中英文均可）：**

| 类别 | 函数 |
|------|------|
| 数学 | SUM/求和、AVERAGE/平均值、MAX/最大值、MIN/最小值、ROUND/四舍五入、ABS/绝对值、SQRT/平方根、POWER/幂、MOD/取余、INT/取整、FLOOR、CEILING、LOG、LN、EXP... |
| 统计 | MEDIAN/中位数、STDEV/标准差、VAR/方差、LARGE、SMALL |
| 文本 | LEN/长度、LEFT/取左、RIGHT/取右、MID/取中、UPPER、LOWER、TRIM/去空格、CONCAT/拼接、FIND/查找、SUBSTITUTE/替换 |
| 条件 | IF(条件, 真值, 假值)、IFS/多条件(条件1, 值1, ..., 默认值) |
| 逻辑 | AND/且、OR/或、NOT/非、XOR/异或 |
| 日期 | TODAY/今天、NOW/现在、YEAR/年、MONTH/月、DAY/日、DAYS/相差天数 |

> 聚合函数（如 `MAX(总分)`）自动计算整列的最大值，而非当前行的值。

### 📊 统计分析
- 整体统计：合计、均值、最大、最小
- 分组统计：按指定列分组，对各指标分别汇总

### 💾 导出 xlsx
- 表头深蓝加粗，自动列宽（中文字符按实际显示宽度计算）
- 分级列自动颜色标注（优秀=绿 / 良好=黄 / 待改进=红）
- 支持多 Sheet 导出（计算结果 + 分组统计）

### 📄 导出 PDF
- 动态报告标题（AI 自动推荐，可手动修改）
- 三个章节：数据明细 / 整体统计 / 分组统计
- 隔行浅蓝 + 分级颜色标注
- 自动分页，紧凑排列

### 🤖 AI 智能分析
- 自动分析列名和样本数据，推荐最有价值的公式
- 自动配置统计列和分组列
- 自动推荐报告标题
- 支持**格式要求**输入（指定分级标准、保留小数位等）
- 验证失败时可一键让 AI **自动修复**语法错误
- 支持保存/加载规则文件（`.json`）

---

## 🤖 支持的 AI 接口

| 服务商 | 格式 | 说明 |
|--------|------|------|
| Anthropic Claude | Anthropic | 官方 API |
| OpenAI GPT | OpenAI 兼容 | 官方 API |
| DeepSeek | OpenAI 兼容 | 国产，性价比高 |
| 阿里通义千问 | OpenAI 兼容 | 国产 |
| Ollama 本地 | OpenAI 兼容 | 免费，需本地部署 |
| 自定义 | 任意 | 任何兼容接口 |

> **🔒 安全说明**：API Key 保存在 `C:/ExcelProcessorConfig/api_config.json`（项目目录外），不会被 git 提交。

---

## 🖥️ 使用流程

```
1. 打开 xlsx 文件
        ↓
2. 点击【⚙️ 配置规则】
   ├── 填写格式要求（可选）
   ├── 点击【🤖 AI 分析推荐】自动生成公式
   │   └── 或手动输入公式
   ├── 验证公式（出错可一键 AI 修复）
   └── 确认统计配置和报告标题
        ↓
3. 点击【▶ 执行计算】
        ↓
4. 导出结果
   ├── 💾 导出 xlsx（带颜色格式）
   └── 📄 导出 PDF（格式化报告）
```

---

## 🛠️ 构建环境

### 依赖
- **Qt 6.x**（MinGW 64-bit 或 MSVC）
- **CMake 3.16+**
- **QXlsx**（已包含在项目中）

### 编译步骤

```bash
# 克隆仓库
git clone https://github.com/你的用户名/ExcelProcessor.git
cd ExcelProcessor

# 克隆 QXlsx 依赖
git clone https://github.com/QtExcel/QXlsx.git

# 编译
cmake -B build -S . -G "Ninja"
cmake --build build

# 运行
./build/ExcelProcessor.exe
```

> Windows 用户请使用 Qt 自带的 **Qt MinGW 终端**执行上述命令，确保 cmake 和 ninja 在 PATH 中。

---

## 📁 项目结构

```
ExcelProcessor/
├── src/
│   ├── main.cpp
│   ├── io/
│   │   ├── XlsxReader      # xlsx 读取（自动探测结构）
│   │   ├── XlsxWriter      # xlsx 写出（带格式/颜色）
│   │   └── PdfExporter     # PDF 报告生成
│   ├── core/
│   │   ├── DataTable       # 内部数据模型
│   │   ├── FormulaEngine   # 公式执行引擎
│   │   └── StatEngine      # 统计汇总引擎
│   └── config/
│       ├── ExprEvaluator   # 表达式解析器（词法+语法分析）
│       ├── FormulaParser   # 公式文本解析
│       ├── RuleEditor      # 规则配置界面
│       ├── AIAssistant     # AI 接口调用
│       └── ApiSettingsDialog # API 设置界面
├── QXlsx/                  # QXlsx 依赖
├── CMakeLists.txt
└── README.md
```

---

## 📸 截图

| 主界面 | 规则配置器 | PDF 报告 |
|--------|-----------|---------|
| 打开文件后显示列信息 | AI 推荐公式 + 统计配置 | 数据明细 + 分组统计 |

---

## 📝 使用示例

### 班级成绩分析
```
总分 = 数学 + 语文 + 英语
平均分 = ROUND(总分 / 3, 1)
最高分 = MAX(总分)
等级 = IFS(总分>=270, "优秀", 总分>=225, "良好", 总分>=180, "及格", "不及格")
```

### 销售业绩分析
```
净销售量 = 销售数量 - 退货数量
平均单价 = ROUND(销售额(元) / 销售数量, 2)
绩效评分 = IFS(销售额(元)>=8000, "优秀", 销售额(元)>=5000, "良好", "待改进")
```

### 工资计算
```
应发工资 = 底薪 + 绩效奖金
税后工资 = ROUND(应发工资 * 0.8, 0)
评级 = IFS(税后工资>=10000, "A级", 税后工资>=6000, "B级", "C级")
```

---

## ⚠️ 注意事项

- API Key 存储在 `C:/ExcelProcessorConfig/api_config.json`，不在项目目录内
- 公式中列名区分大小写，需与文件中的列名完全一致
- `MAX(列名)` / `MIN(列名)` 等聚合函数会计算整列的聚合值

---

## 📄 License

MIT License
