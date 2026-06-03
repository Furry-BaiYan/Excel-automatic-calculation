#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include "../core/DataTable.h"

// 读取配置
struct ReadConfig {
    int  maxScanRows   = 10;    // 自动探测时扫描前N行
    int  maxRows       = -1;    // -1 = 读全部行
    bool allSheets     = false; // true = 读所有Sheet
    QString sheetName  = "";    // 指定Sheet名，空 = 第一个Sheet
};

// 自动探测结果
struct DetectResult {
    int        headerRow   = 0;   // 表头行（0起）
    int        dataStart   = 1;   // 数据起始行
    int        colCount    = 0;   // 有效列数
    QStringList headers;          // 列名列表
    QVector<ColumnType> types;    // 推断的列类型
    bool       success     = false;
    QString    errorMsg;
};

class XlsxReader {
public:
    // 进度回调：(当前文件, 总文件数, 文件名)
    using ProgressCallback = std::function<void(int, int, const QString&)>;

    // 读取单个文件（自动探测结构）
    static DataTable readFile(const QString& filePath,
                              const ReadConfig& config = {},
                              QString* errorMsg = nullptr);

    // 批量读取文件夹内所有 xlsx
    static QVector<DataTable> readFolder(const QString& folderPath,
                                         const ReadConfig& config = {},
                                         ProgressCallback onProgress = nullptr);

    // 批量读取指定文件列表
    static QVector<DataTable> readFiles(const QStringList& filePaths,
                                        const ReadConfig& config = {},
                                        ProgressCallback onProgress = nullptr);

    // 仅探测文件结构，不读全部数据（快速预览）
    static DetectResult detectStructure(const QString& filePath,
                                        const ReadConfig& config = {});

private:
    static DetectResult   _detect(const QString& filePath,
                                   const ReadConfig& config);
    static ColumnType     _inferType(const QVector<QVariant>& samples);
    static bool           _isHeaderRow(const QVector<QVariant>& row);
    static int            _findHeaderRow(const QString& filePath,
                                          int maxScan);
};