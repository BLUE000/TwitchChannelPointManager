#pragma once
#include <QString>

class TranslationReviewer {
public:
    TranslationReviewer() = delete; // 静的ユーティリティクラス

    // translationsDir 配下の app_*.ts を解析し、outputPath に CSV を出力する
    // 開発・デバッグ環境でのみ動作し、ディレクトリが見つからない場合は単に false を返して無視する
    static bool generateReviewCsv(const QString& translationsDir, const QString& outputPath);
};
