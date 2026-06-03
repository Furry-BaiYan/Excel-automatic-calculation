#pragma once
#include <QString>
#include "../core/DataTable.h"
#include "../core/StatEngine.h"

class PdfExporter {
public:
    // 导出完整报告：标题 + 数据表 + 整体统计 + 分组统计
    static bool exportReport(const DataTable&  dataTable,
                             const StatReport& report,
                             const QString&    filePath,
                             QString*          errorMsg = nullptr);
};