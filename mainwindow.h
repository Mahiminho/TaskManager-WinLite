#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMessageBox>
#include "qcustomplot.h"

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <unordered_map>
#include <shlwapi.h>
#include <cmath>
#include <processthreadsapi.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "Shlwapi.lib")

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    std::vector<std::string> procNames;
    std::vector<double> procCPU;
    std::vector<double> procRAM;
    std::vector<double> procDisk;

    std::vector<int> procIDs;
    std::vector<std::string> procParent;
    std::vector<int> procPriority;
    std::vector<int> procThreads;
    std::vector<int> procStatus;

    double maxRAM;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int getProcessorNumber();
    __int64 fileTime2UTC(const FILETIME* ftime);
    double getCpuUsage(int pid);
    double getMaxRam();
    double getRamForProcess(HANDLE hProcess);
    double getExecStatus(HANDLE hProcess);
    std::string getProcessName(DWORD processId);
    double getDiskUsage(HANDLE hProcess);
    void getListsOfProcessesInfo(double &maxRAMmb, std::vector<std::string> &procNames, std::vector<int> &procIDs, std::vector<double>& procCPU,
        std::vector<double>& procRAM, std::vector<int>& procThreads, std::vector<int>& procPriority, std::vector<int>& procStatus, std::vector<std::string> &procParent,
        std::vector<double>& procGPU);

    void fillProcTable();
    void updateModeCheck();
    void fillDetTable();

    double cpuVal;
    double ramVal;
    double diskVal;
    double time;

    void updateDiagram();

private slots:
    void on_pushButtonUpdate_clicked();
    void on_pushButtonSus_clicked();
    void on_pushButtonRes_clicked();
    void on_pushButtonTer_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
