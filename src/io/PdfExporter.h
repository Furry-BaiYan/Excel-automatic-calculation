#pragma once
#include <QString>
#include "../core/DataTable.h"
#include "../core/StatEngine.h"

class PdfExporter {
public:
    static bool exportReport(const DataTable&  dataTable,
                             const StatReport& report,
                             const QString&    filePath,
                             const QString&    title    = "",
                             QString*          errorMsg = nullptr);
};