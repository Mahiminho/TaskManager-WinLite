#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->radioButtonAuto->setChecked(true);

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateModeCheck);
    timer->start(1000);

    QTimer* timer2 = new QTimer(this);
    connect(timer2, &QTimer::timeout, this, &MainWindow::fillDetTable);
    timer2->start(1010);

    // Setup graphs
    ui->cpuPlot->addGraph();
    ui->cpuPlot->xAxis->setLabel("Time(s)");
    ui->cpuPlot->xAxis->setRange(0, 60);
    ui->cpuPlot->yAxis->setLabel("Percent(%)");
    ui->cpuPlot->yAxis->setRange(0, 100);

    ui->ramPlot->addGraph();
    ui->ramPlot->xAxis->setLabel("Time(s)");
    ui->ramPlot->xAxis->setRange(0, 60);
    ui->ramPlot->yAxis->setLabel("Percent(%)");
    ui->ramPlot->yAxis->setRange(0, 100);

    ui->diskPlot->addGraph();
    ui->diskPlot->xAxis->setLabel("Time(s)");
    ui->diskPlot->xAxis->setRange(0, 60);
    ui->diskPlot->yAxis->setLabel("Percent(%)");
    ui->diskPlot->yAxis->setRange(0, 100);

    time = 0.0;
    QTimer* timerCPU = new QTimer(this);
    connect(timerCPU, &QTimer::timeout, this, &MainWindow::updateDiagram);
    timerCPU->start(1000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateDiagram()
{
    time++;
    ui->cpuPlot->graph(0)->addData(time, cpuVal);
    ui->ramPlot->graph(0)->addData(time, ramVal / maxRAM * 100);
    ui->diskPlot->graph(0)->addData(time, diskVal);
    if (time > 60)
    {
        ui->cpuPlot->xAxis->setRange(time - 50, time + 10);
        ui->ramPlot->xAxis->setRange(time - 50, time + 10);
        ui->diskPlot->xAxis->setRange(time - 50, time + 10);
    }
    ui->cpuPlot->replot();
    ui->ramPlot->replot();
    ui->diskPlot->replot();
}

void MainWindow::updateModeCheck()
{
    if (ui->radioButtonAuto->isChecked())
    {
        ui->pushButtonUpdate->setEnabled(false);
        fillProcTable();
    }
    else
    {
        ui->pushButtonUpdate->setEnabled(true);
    }
}

void MainWindow::on_pushButtonUpdate_clicked()
{
    fillProcTable();
}

int MainWindow::getProcessorNumber()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

__int64 MainWindow::fileTime2UTC(const FILETIME* ftime)
{
    LARGE_INTEGER li;
    li.LowPart = ftime->dwLowDateTime;
    li.HighPart = ftime->dwHighDateTime;
    return li.QuadPart;
}

double MainWindow::getCpuUsage(int pid)
{
    static int processor_count_ = -1;
    static std::unordered_map<int, __int64> last_time_;
    static std::unordered_map<int, __int64> last_system_time_;
    FILETIME now;
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    __int64 system_time;
    __int64 time;
    __int64 system_time_delta;
    __int64 time_delta;
    double cpu = -1;
    if (processor_count_ == -1)
    {
        processor_count_ = getProcessorNumber();
    }
    GetSystemTimeAsFileTime(&now);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
    if (!GetProcessTimes(hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
    {
        return 0;
    }
    system_time = (fileTime2UTC(&kernel_time) + fileTime2UTC(&user_time)) / processor_count_;
    time = fileTime2UTC(&now);
    if ((last_system_time_[pid] == 0) || (last_time_[pid] == 0))
    {
        last_system_time_[pid] = system_time;
        last_time_[pid] = time;
        return 0;
    }
    system_time_delta = system_time - last_system_time_[pid];
    time_delta = time - last_time_[pid];
    if (time_delta == 0)
    {
        return 0;
    }
    cpu = double((system_time_delta * 100 + time_delta / 2) / time_delta);
    last_system_time_[pid] = system_time;
    last_time_[pid] = time;
    return cpu;
}

double MainWindow::getMaxRam()
{
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    GlobalMemoryStatusEx(&memoryStatus);
    DWORDLONG totalMemory = memoryStatus.ullTotalPhys;
    double totalMemoryInB = static_cast<double>(totalMemory);
    double totalMemoryInMB = std::round(totalMemoryInB / (1024.0 * 1024.0) * 10.0) / 10.0;
    return totalMemoryInMB;
}

double MainWindow::getRamForProcess(HANDLE hProcess)
{
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
    double memoryUsage = static_cast<double>(pmc.WorkingSetSize);
    double memoryMB = memoryUsage / (1024.0 * 1024.0);
    double memoryMBRounded = std::round(memoryMB * 10) / 10;
    if (memoryMBRounded > 3000.0)
        return 0;
    else
        return memoryMBRounded;
}

double MainWindow::getExecStatus(HANDLE hProcess)
{
    DWORD exitCode;
    GetExitCodeProcess(hProcess, &exitCode); // 259 - still alive, else - terminated
    int execStatus = static_cast<int>(exitCode);
    if (execStatus < 0)
        return 0;
    else
        return execStatus;
}

std::string MainWindow::getProcessName(DWORD processId)
{
    std::string ret;
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (handle)
    {
        CHAR buffer[MAX_PATH];
        DWORD buffSize = sizeof(buffer);
        if (GetProcessImageFileNameA(handle, buffer, buffSize) > 0)
        {
            PathStripPathA(buffer);
            ret = buffer;
        }
        CloseHandle(handle);
    }
    else
    {
        ret = "-";
    }
    return ret;
}

double MainWindow::getDiskUsage(HANDLE hProcess)
{
    IO_COUNTERS ioCounters;
    double diskUsagePercent = 0.0;

    if (GetProcessIoCounters(hProcess, &ioCounters) != 0)
    {
        ULONGLONG totalDiskIO = ioCounters.ReadOperationCount + ioCounters.WriteOperationCount;
        ULONGLONG totalSystemDiskIO = 0;
        PERFORMANCE_INFORMATION performanceInfo;
        if (GetPerformanceInfo(&performanceInfo, sizeof(performanceInfo)))
        {
            totalSystemDiskIO = performanceInfo.CommitTotal * performanceInfo.PageSize;
        }
        if (totalSystemDiskIO > 0)
        {
            diskUsagePercent = static_cast<double>(totalDiskIO) / totalSystemDiskIO * 100.0;
        }
    }
    else
    {
        diskUsagePercent = 0;
    }

    diskUsagePercent = std::round(diskUsagePercent * 10.0) / 10.0;
    return diskUsagePercent;
}

void MainWindow::getListsOfProcessesInfo(double &maxRAMmb, std::vector<std::string> &procNames, std::vector<int> &procIDs, std::vector<double>& procCPU,
    std::vector<double>& procRAM, std::vector<int>& procThreads, std::vector<int>& procPriority, std::vector<int>& procStatus, std::vector<std::string> &procParent,
    std::vector<double>& procDisk)
{
    // Struct PROCESSENTRY32
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);

    // Get descriptor for process list
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSnapshot);
        return;
    }

    // Get first process
    if (!Process32First(hSnapshot, &processInfo))
    {
        CloseHandle(hSnapshot);
        return;
    }

    // Get maximum memory in MB
    maxRAMmb = getMaxRam();

    do
    {
        // Descriptor for process information
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processInfo.th32ProcessID);

        // Get process name and PID
        std::wstring processName(processInfo.szExeFile);
        std::string processNameString(processName.begin(), processName.end());
        procNames.push_back(processNameString);
        procIDs.push_back(processInfo.th32ProcessID);

        // Get process threads and priority
        int threadsCount = static_cast<int>(processInfo.cntThreads);
        int basePriority = static_cast<int>(processInfo.pcPriClassBase);
        procThreads.push_back(processInfo.cntThreads);
        procPriority.push_back(processInfo.pcPriClassBase);

        // Get process host name and exec status
        procParent.push_back(getProcessName(processInfo.th32ParentProcessID));
        procStatus.push_back(getExecStatus(hProcess));

        // Get process CPU in percentage
        procCPU.push_back(getCpuUsage(processInfo.th32ProcessID));

        // Get process RAM usage in MB
        procRAM.push_back(getRamForProcess(hProcess));

        // Get disk usage im percentage
        procDisk.push_back(getDiskUsage(hProcess));

        CloseHandle(hProcess);
    }
    while (Process32Next(hSnapshot, &processInfo));

    CloseHandle(hSnapshot);
}

void MainWindow::fillProcTable()
{
    // Clear vectors
    procPriority.clear();
    procThreads.clear();
    procNames.clear();
    procIDs.clear();
    procCPU.clear();
    procRAM.clear();
    procStatus.clear();
    procParent.clear();
    procDisk.clear();

    // Get process info
    getListsOfProcessesInfo(maxRAM, procNames, procIDs, procCPU, procRAM, procThreads, procPriority, procStatus, procParent, procDisk);
    double totalCPU = std::accumulate(procCPU.begin(), procCPU.end(), 0.0);
    double totalRAM = std::accumulate(procRAM.begin(), procRAM.end(), 0.0);
    double totalDisk = std::accumulate(procDisk.begin(), procDisk.end(), 0.0);

    cpuVal = totalCPU;
    ramVal = totalRAM;
    diskVal = totalDisk;

    // Set table from page 1 subname and clear it
    QTableWidget *table1 = ui->tableWidgetProc;
    table1->setRowCount(0);

    // Set 4 rows
    ui->tableWidgetProc->setColumnCount(4);

    // Set column names
    QString cpuTitle = "CPU(" + QString::number(totalCPU) + "%)";
    QString ramTitle = "RAM(" + QString::number(totalRAM) + "/" + QString::number(maxRAM) + "MB)";
    QString diskTitle = "Disk(" + QString::number(totalDisk) + "%)";
    QStringList headerLabels = {"Process name", cpuTitle, ramTitle, diskTitle};
    table1->setHorizontalHeaderLabels(headerLabels);

    // Set static data in table
    table1->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Columns and rows names to bold
    QFont font = table1->horizontalHeader()->font();
    font.setBold(true);
    font.setPointSize(14);
    table1->horizontalHeader()->setFont(font);
    font = table1->verticalHeader()->font();
    font.setBold(true);
    table1->verticalHeader()->setFont(font);

    // Fill table
    for (int row = 0; row < procNames.size(); row++) {
        // Fill main table from page 1
        table1->insertRow(ui->tableWidgetProc->rowCount());
        int lastRow = table1->rowCount() - 1;
        table1->setItem(lastRow, 0, new QTableWidgetItem(QString::fromStdString(procNames[row])));
        table1->setItem(lastRow, 1, new QTableWidgetItem(QString::number(procCPU[row]) + "%"));
        table1->setItem(lastRow, 2, new QTableWidgetItem(QString::number(procRAM[row]) + "MB"));
        table1->setItem(lastRow, 3, new QTableWidgetItem(QString::number(procDisk[row]) + "%"));

        QBrush cpuColor = QBrush(QColor::fromHsv((100 - procCPU[row]) * 60 / 100, 165, 255));
        QBrush ramColor = QBrush(QColor::fromHsv((100 - procRAM[row] / 8100 * 100) * 60 / 100, 165, 255));
        QBrush diskColor = QBrush(QColor::fromHsv((100 - procDisk[row]) * 60 / 100, 165, 255));

        table1->item(row, 0)->setBackground(QColor::fromHsv(0, 0, 255));
        table1->item(row, 1)->setBackground(cpuColor);
        table1->item(row, 2)->setBackground(ramColor);
        table1->item(row, 3)->setBackground(diskColor);
    }

    // Make rows full width
    table1->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Select only row
    table1->setSelectionBehavior(QAbstractItemView::SelectRows);
    table1->setSelectionMode(QAbstractItemView::SingleSelection);
}

void MainWindow::on_pushButtonSus_clicked()
{
    int index = ui->tableWidgetProc->currentRow();
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, DWORD(procIDs[index]));
    if (hProcess == NULL)
    {
        QMessageBox::critical(this, "Error!", "(1) Unenable to suspend process " + QString::fromStdString(procNames[index]) + "!");
    }

    bool success = (SuspendThread(hProcess) != (DWORD) - 1);
    if (success)
    {
        QMessageBox::information(this, "Information.", "Process " + QString::fromStdString(procNames[index]) + " has been successfully suspended.");
    }
    else
    {
        QMessageBox::critical(this, "Error!", "(2) Unenable to suspend process " + QString::fromStdString(procNames[index]) + "!");
    }

    fillProcTable();
    CloseHandle(hProcess);
}

void MainWindow::on_pushButtonRes_clicked()
{
    int index = ui->tableWidgetProc->currentRow();
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, DWORD(procIDs[index]));
    if (hProcess == NULL)
    {
        QMessageBox::critical(this, "Error!", "(1) Unenable to resume process " + QString::fromStdString(procNames[index]) + "!");
    }

    bool success = (ResumeThread(hProcess) != (DWORD)-1);
    if (success)
    {
        QMessageBox::information(this, "Information.", "Process " + QString::fromStdString(procNames[index]) + " has been successfully resumed.");
    }
    else
    {
        QMessageBox::critical(this, "Error!", "(2) Unenable to resume process " + QString::fromStdString(procNames[index]) + "!");
    }

    fillProcTable();
    CloseHandle(hProcess);
}

void MainWindow::on_pushButtonTer_clicked()
{
    int index = ui->tableWidgetProc->currentRow();
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, DWORD(procIDs[index]));
    if (hProcess == NULL)
    {
        QMessageBox::critical(this, "Error!", "(1) Unenable to terminate process " + QString::fromStdString(procNames[index]) + "!");
    }

    UINT uExitCode;
    bool success = (TerminateProcess(hProcess, uExitCode) != 0);
    if (success)
    {
        QMessageBox::information(this, "Information.", "Process " + QString::fromStdString(procNames[index]) + " has been successfully terminated.");
    }
    else
    {
        QMessageBox::critical(this, "Error!", "(2) Unenable to terminate process " + QString::fromStdString(procNames[index]) + "!");
    }

    fillProcTable();
    CloseHandle(hProcess);
}

void MainWindow::fillDetTable()
{
    // Clear vectors
    procPriority.clear();
    procThreads.clear();
    procNames.clear();
    procIDs.clear();
    procCPU.clear();
    procRAM.clear();
    procStatus.clear();
    procParent.clear();
    procDisk.clear();

    // Get process info
    getListsOfProcessesInfo(maxRAM, procNames, procIDs, procCPU, procRAM, procThreads, procPriority, procStatus, procParent, procDisk);

    // Set table from page 1 subname and clear it
    QTableWidget *table1 = ui->tableWidgetDet;
    table1->setRowCount(0);

    // Set 4 rows
    ui->tableWidgetDet->setColumnCount(6);

    // Set column names
    QStringList headerLabels = {"Process name", "PID", "Threads", "Host name", "Priority", "Status"};
    table1->setHorizontalHeaderLabels(headerLabels);

    // Set static data in table
    table1->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Columns and rows names to bold
    QFont font = table1->horizontalHeader()->font();
    font.setBold(true);
    font.setPointSize(14);
    table1->horizontalHeader()->setFont(font);
    font = table1->verticalHeader()->font();
    font.setBold(true);
    table1->verticalHeader()->setFont(font);

    // Fill table
    for (int row = 0; row < procNames.size(); row++) {
        // Fill main table from page 1
        table1->insertRow(ui->tableWidgetDet->rowCount());
        int lastRow = table1->rowCount() - 1;
        table1->setItem(lastRow, 0, new QTableWidgetItem(QString::fromStdString(procNames[row])));
        table1->setItem(lastRow, 1, new QTableWidgetItem(QString::number(procIDs[row])));
        table1->setItem(lastRow, 2, new QTableWidgetItem(QString::number(procThreads[row])));
        table1->setItem(lastRow, 3, new QTableWidgetItem(QString::fromStdString(procParent[row])));
        table1->setItem(lastRow, 4, new QTableWidgetItem(QString::number(procPriority[row])));
        QString execStatus = (procStatus[row] == 259) ? "In progress" : "Suspended";
        table1->setItem(lastRow, 5, new QTableWidgetItem(execStatus));

        table1->item(row, 0)->setBackground(QColor::fromHsv(0, 0, 200));
        table1->item(row, 1)->setBackground(QColor::fromHsv(0, 0, 200));
        table1->item(row, 2)->setBackground(QColor::fromHsv(0, 0, 200));
        table1->item(row, 3)->setBackground(QColor::fromHsv(0, 0, 200));
        table1->item(row, 4)->setBackground(QColor::fromHsv(0, 0, 200));
        table1->item(row, 5)->setBackground(QColor::fromHsv(0, 0, 200));
    }

    // Make rows full width
    table1->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Select only row
    table1->setSelectionBehavior(QAbstractItemView::SelectRows);
    table1->setSelectionMode(QAbstractItemView::SingleSelection);
}
