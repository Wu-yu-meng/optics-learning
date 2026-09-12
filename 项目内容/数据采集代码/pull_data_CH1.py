import sys
import pyvisa
import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtWidgets

# ===================== 查询并连接设备 =====================
rm = pyvisa.ResourceManager()
print(rm.list_resources())   # 查看设备地址

inst0 = rm.open_resource('TCPIP::192.168.1.3::INSTR')
inst0.timeout = 20000

try:
    print(inst0.query('*IDN?'))
except pyvisa.errors.VisaIOError as e:
    print(f"通信失败: {e}")
    sys.exit(1)

# ===================== 获取数据 =====================
TABLE_ROWS = 1000            # 表格只显示前 1000 个点

def GET_data():
    inst0.write(':DATa:SOUrce CH1')          # 先选定信源，否则 WFMOutpre 可能不响应
    inst0.write(':DATa:STARt 1')
    inst0.write(':DATa:STOP 100000')

    record = int(inst0.query('horizontal:recordlength?').split()[-1])
    print(f"记录长度: {record}")

    tscale = float(inst0.query('WFMOutpre:XINcr?').split()[-1])
    tstart = float(inst0.query('WFMOutpre:XZEro?').split()[-1])
    vscale = float(inst0.query('WFMOutpre:YMULt?').split()[-1])
    voff   = float(inst0.query('WFMOutpre:YZEro?').split()[-1])
    vpos   = float(inst0.query('WFMOutpre:YOFf?').split()[-1])
    print(tscale, tstart, vscale, voff, vpos)

    # ---------------- 界面：左图右表 ----------------
    app = QtWidgets.QApplication([])

    win = QtWidgets.QWidget()
    win.setWindowTitle("Live Waveform - CH1")
    win.resize(1400, 500)
    layout = QtWidgets.QHBoxLayout()
    win.setLayout(layout)

    # 左：波形图
    glw = pg.GraphicsLayoutWidget()
    layout.addWidget(glw, 3)
    plot = glw.addPlot(title="CH1")
    plot.showGrid(x=True, y=True, alpha=0.3)
    plot.setLabel('left', 'Voltage', units='V')
    plot.setLabel('bottom', 'Time', units='s')
    curve = plot.plot(pen=pg.mkPen('y', width=1))
    curve.setClipToView(True)
    curve.setDownsampling(auto=True, method='peak')

    # 右：数据表格（固定 1000 行）
    table = QtWidgets.QTableWidget(TABLE_ROWS, 3)
    table.setHorizontalHeaderLabels(['Index', 'Time (s)', 'Voltage (V)'])
    table.horizontalHeader().setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
    table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
    table.setAlternatingRowColors(True)
    layout.addWidget(table, 2)

    win.show()

    x_full = np.arange(record) * tscale + tstart   # 真实时间轴

    # ---------------- 定时采集 + 实时刷新 ----------------
    count = 0

    def acquire():
        nonlocal count
        try:
            bin_wave = inst0.query_binary_values('CURVE?', datatype='h', container=np.array)
        except pyvisa.errors.VisaIOError:
            return                            # 偶尔超时，跳过这一帧

        scaled_wave = (np.array(bin_wave, dtype='double') - vpos) * vscale + voff
        curve.setData(x_full[:len(scaled_wave)], scaled_wave)

        # 表格只显示前 TABLE_ROWS 个点
        n_show = min(TABLE_ROWS, len(scaled_wave))
        table.setRowCount(n_show)
        for r in range(n_show):
            table.setItem(r, 0, QtWidgets.QTableWidgetItem(str(r)))
            table.setItem(r, 1, QtWidgets.QTableWidgetItem(f"{x_full[r]:.6e}"))
            table.setItem(r, 2, QtWidgets.QTableWidgetItem(f"{scaled_wave[r]:.6f}"))
        table.scrollToTop()

        count += 1

    timer = QtCore.QTimer()
    timer.timeout.connect(acquire)
    timer.start(500)     # 每 500ms 采集一帧

    def on_exit():
        timer.stop()
        inst0.close()
        rm.close()
        print(f"\n共采集 {count} 帧")

    app.aboutToQuit.connect(on_exit)
    app.exec_()

if __name__ == '__main__':
    GET_data()