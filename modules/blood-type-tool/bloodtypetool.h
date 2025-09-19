#ifndef BLOODTYPETOOL_H
#define BLOODTYPETOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QRect>
#include <QRandomGenerator>
#include <QDateTime>
#include <QMap>
#include <QStringList>

#include "../../common/dynamicobjectbase.h"

// ABO血型系统枚举
enum class ABOBloodType {
    A,      // A型血
    B,      // B型血
    AB,     // AB型血
    O       // O型血
};

// Rh血型系统枚举
enum class RhBloodType {
    Positive,   // Rh阳性
    Negative    // Rh阴性
};

// 基因型结构（ABO系统）
struct ABOGenotype {
    char allele1;   // 第一个等位基因 (A, B, O)
    char allele2;   // 第二个等位基因 (A, B, O)

    ABOGenotype(char a1 = 'O', char a2 = 'O') : allele1(a1), allele2(a2) {
        // 确保基因排序一致性
        if (allele1 > allele2) {
            std::swap(allele1, allele2);
        }
    }

    ABOBloodType getBloodType() const {
        if ((allele1 == 'A' && allele2 == 'A') || (allele1 == 'A' && allele2 == 'O') || (allele1 == 'O' && allele2 == 'A')) {
            return ABOBloodType::A;
        } else if ((allele1 == 'B' && allele2 == 'B') || (allele1 == 'B' && allele2 == 'O') || (allele1 == 'O' && allele2 == 'B')) {
            return ABOBloodType::B;
        } else if (allele1 == 'A' && allele2 == 'B') {
            return ABOBloodType::AB;
        } else {
            return ABOBloodType::O;
        }
    }

    QString toString() const {
        return QString("%1%2").arg(allele1).arg(allele2);
    }

    bool operator==(const ABOGenotype& other) const {
        return allele1 == other.allele1 && allele2 == other.allele2;
    }
};

// Rh基因型结构
struct RhGenotype {
    bool allele1;   // 第一个等位基因 (true=D, false=d)
    bool allele2;   // 第二个等位基因 (true=D, false=d)

    RhGenotype(bool a1 = false, bool a2 = false) : allele1(a1), allele2(a2) {}

    RhBloodType getBloodType() const {
        return (allele1 || allele2) ? RhBloodType::Positive : RhBloodType::Negative;
    }

    QString toString() const {
        char a1 = allele1 ? 'D' : 'd';
        char a2 = allele2 ? 'D' : 'd';
        return QString("%1%2").arg(a1).arg(a2);
    }

    bool operator==(const RhGenotype& other) const {
        return allele1 == other.allele1 && allele2 == other.allele2;
    }
};

// 完整血型结构
struct CompleteBloodType {
    ABOGenotype aboGenotype;
    RhGenotype rhGenotype;

    CompleteBloodType(const ABOGenotype& abo = ABOGenotype(), const RhGenotype& rh = RhGenotype())
        : aboGenotype(abo), rhGenotype(rh) {}

    QString getPhenotypeString() const {
        QString aboStr;
        switch (aboGenotype.getBloodType()) {
            case ABOBloodType::A: aboStr = "A"; break;
            case ABOBloodType::B: aboStr = "B"; break;
            case ABOBloodType::AB: aboStr = "AB"; break;
            case ABOBloodType::O: aboStr = "O"; break;
        }

        QString rhStr = (rhGenotype.getBloodType() == RhBloodType::Positive) ? "+" : "-";
        return aboStr + rhStr;
    }

    QString getGenotypeString() const {
        return QString("%1 %2").arg(aboGenotype.toString(), rhGenotype.toString());
    }
};

// 遗传结果项
struct HeritageResult {
    CompleteBloodType bloodType;
    double probability;     // 概率(0-1)
    int count;             // 出现次数
    QString description;   // 描述

    HeritageResult(const CompleteBloodType& bt = CompleteBloodType(), double prob = 0.0, int cnt = 0)
        : bloodType(bt), probability(prob), count(cnt) {}
};

// 血型遗传计算器
class BloodTypeCalculator {
public:
    static QList<HeritageResult> calculateOffspring(const CompleteBloodType& parent1, const CompleteBloodType& parent2);
    static QList<CompleteBloodType> getPossibleParents(const CompleteBloodType& child, const CompleteBloodType& knownParent);
    static bool isPossibleChild(const CompleteBloodType& parent1, const CompleteBloodType& parent2, const CompleteBloodType& child);
    static QList<ABOGenotype> getAllPossibleABOGenotypes(ABOBloodType bloodType);
    static QList<RhGenotype> getAllPossibleRhGenotypes(RhBloodType bloodType);
    static QString getBloodTypeDescription(const CompleteBloodType& bloodType);
    static QStringList getCompatibleDonors(const CompleteBloodType& recipient);
    static QStringList getCompatibleRecipients(const CompleteBloodType& donor);

private:
    static QList<ABOGenotype> crossABO(const ABOGenotype& parent1, const ABOGenotype& parent2);
    static QList<RhGenotype> crossRh(const RhGenotype& parent1, const RhGenotype& parent2);
};

// 血型遗传计算组件
class BloodTypeCalculator_Widget : public QWidget {
    Q_OBJECT

public:
    explicit BloodTypeCalculator_Widget(QWidget* parent = nullptr);

private slots:
    void onCalculateClicked();
    void onClearClicked();
    void onCopyResultClicked();
    void onSaveResultClicked();
    void onParentBloodTypeChanged();

private:
    void setupUI();
    void updateUI();
    void displayResults(const QList<HeritageResult>& results);
    QString bloodTypeToString(ABOBloodType type);
    QString rhTypeToString(RhBloodType type);
    ABOBloodType stringToBloodType(const QString& str);
    RhBloodType stringToRhType(const QString& str);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 输入区域
    QWidget* m_inputWidget;
    QVBoxLayout* m_inputLayout;

    QGroupBox* m_parent1Group;
    QFormLayout* m_parent1Layout;
    QComboBox* m_parent1ABOCombo;
    QComboBox* m_parent1RhCombo;

    QGroupBox* m_parent2Group;
    QFormLayout* m_parent2Layout;
    QComboBox* m_parent2ABOCombo;
    QComboBox* m_parent2RhCombo;

    QGroupBox* m_controlGroup;
    QVBoxLayout* m_controlLayout;
    QPushButton* m_calculateBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_copyResultBtn;
    QPushButton* m_saveResultBtn;

    // 结果显示区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;
    QGroupBox* m_resultGroup;
    QVBoxLayout* m_resultGroupLayout;
    QTableWidget* m_resultTable;

    // 当前结果
    QList<HeritageResult> m_lastResults;
};

// 血型知识百科组件
class BloodTypeKnowledge : public QWidget {
    Q_OBJECT

public:
    explicit BloodTypeKnowledge(QWidget* parent = nullptr);

private slots:
    void onTopicChanged();
    void onCopyContentClicked();
    void onSaveContentClicked();

private:
    void setupUI();
    void loadKnowledgeContent();
    void displayContent(const QString& topic);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_topLayout;

    QGroupBox* m_topicGroup;
    QVBoxLayout* m_topicLayout;
    QComboBox* m_topicCombo;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;

    QGroupBox* m_contentGroup;
    QVBoxLayout* m_contentLayout;
    QTextEdit* m_contentText;

    // 知识内容
    QMap<QString, QString> m_knowledgeContent;
};

// 血型配对分析组件
class BloodTypeCompatibility : public QWidget {
    Q_OBJECT

public:
    explicit BloodTypeCompatibility(QWidget* parent = nullptr);

private slots:
    void onCheckCompatibilityClicked();
    void onClearClicked();
    void onBloodTypeChanged();

private:
    void setupUI();
    void updateUI();
    void displayCompatibilityResult(const QString& donorType, const QString& recipientType, bool compatible);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 输入区域
    QWidget* m_inputWidget;
    QVBoxLayout* m_inputLayout;

    QGroupBox* m_donorGroup;
    QFormLayout* m_donorLayout;
    QComboBox* m_donorABOCombo;
    QComboBox* m_donorRhCombo;
    QLabel* m_donorTypeLabel;

    QGroupBox* m_recipientGroup;
    QFormLayout* m_recipientLayout;
    QComboBox* m_recipientABOCombo;
    QComboBox* m_recipientRhCombo;
    QLabel* m_recipientTypeLabel;

    QGroupBox* m_controlGroup;
    QVBoxLayout* m_controlLayout;
    QPushButton* m_checkBtn;
    QPushButton* m_clearBtn;

    // 结果显示区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;

    QGroupBox* m_compatibilityGroup;
    QVBoxLayout* m_compatibilityLayout;
    QLabel* m_compatibilityLabel;
    QLabel* m_compatibilityIcon;
    QTextEdit* m_compatibilityDetails;

    QGroupBox* m_donorCompatibleGroup;
    QVBoxLayout* m_donorCompatibleLayout;
    QLabel* m_donorCompatibleLabel;
    QTextEdit* m_donorCompatibleList;

    QGroupBox* m_recipientCompatibleGroup;
    QVBoxLayout* m_recipientCompatibleLayout;
    QLabel* m_recipientCompatibleLabel;
    QTextEdit* m_recipientCompatibleList;
};

// 血型统计分析组件
class BloodTypeStatistics : public QWidget {
    Q_OBJECT

public:
    explicit BloodTypeStatistics(QWidget* parent = nullptr);

private slots:
    void onGenerateStatsClicked();
    void onClearStatsClicked();
    void onExportStatsClicked();
    void onRegionChanged();

private:
    void setupUI();
    void generateStatistics();
    void displayStatistics();
    void drawBloodTypeChart(QPainter& painter, const QRect& rect, const QMap<QString, double>& data);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 控制区域
    QWidget* m_controlWidget;
    QVBoxLayout* m_controlLayout;

    QGroupBox* m_regionGroup;
    QFormLayout* m_regionLayout;
    QComboBox* m_regionCombo;

    QGroupBox* m_actionGroup;
    QVBoxLayout* m_actionLayout;
    QPushButton* m_generateBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;

    // 统计显示区域
    QWidget* m_statsWidget;
    QVBoxLayout* m_statsLayout;

    QGroupBox* m_chartGroup;
    QVBoxLayout* m_chartLayout;
    QLabel* m_chartLabel;

    QGroupBox* m_tableGroup;
    QVBoxLayout* m_tableLayout;
    QTableWidget* m_statsTable;

    // 统计数据
    QMap<QString, QMap<QString, double>> m_regionalStats;
    QMap<QString, double> m_currentStats;
};

// 血型遗传模拟组件
class BloodTypeSimulation : public QWidget {
    Q_OBJECT

public:
    explicit BloodTypeSimulation(QWidget* parent = nullptr);

private slots:
    void onRunSimulationClicked();
    void onClearSimulationClicked();
    void onSaveSimulationClicked();
    void onGenerationCountChanged();

private:
    void setupUI();
    void runGeneticSimulation();
    void displaySimulationResults();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 参数设置区域
    QWidget* m_paramWidget;
    QVBoxLayout* m_paramLayout;

    QGroupBox* m_initialGroup;
    QFormLayout* m_initialLayout;
    QComboBox* m_initialABOCombo;
    QComboBox* m_initialRhCombo;

    QGroupBox* m_simulationGroup;
    QFormLayout* m_simulationLayout;
    QComboBox* m_generationsCombo;
    QComboBox* m_populationCombo;

    QGroupBox* m_controlGroup;
    QVBoxLayout* m_controlLayout;
    QPushButton* m_runBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_saveBtn;

    // 结果显示区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;

    QGroupBox* m_generationsGroup;
    QVBoxLayout* m_generationsLayout;
    QTableWidget* m_generationsTable;

    QGroupBox* m_finalGroup;
    QVBoxLayout* m_finalLayout;
    QTextEdit* m_finalResults;

    // 模拟数据
    QList<QMap<QString, int>> m_simulationData;
};

// 血型工具主类
class BloodTypeTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit BloodTypeTool(QWidget* parent = nullptr);
    ~BloodTypeTool() override = default;

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    BloodTypeCalculator_Widget* m_calculator;
    BloodTypeKnowledge* m_knowledge;
    BloodTypeCompatibility* m_compatibility;
    BloodTypeStatistics* m_statistics;
    BloodTypeSimulation* m_simulation;
};

#endif // BLOODTYPETOOL_H