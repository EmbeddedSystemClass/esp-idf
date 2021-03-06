配置其它 JTAG 接口
==================
:link_to_translation:`en:[English]`

关于适配 OpenOCD 和 {IDF_TARGET_NAME} 的 JTAG 接口选择问题，请参考 :ref:`jtag-debugging-selecting-jtag-adapter` 章节，确保 JTAG 适配器能够与 OpenOCD 和 {IDF_TARGET_NAME} 一同工作。然后按照以下三个步骤进行设置，使其正常工作。


配置硬件
^^^^^^^^

1.  找到 JTAG 接口和 {IDF_TARGET_NAME} 板上需要相互连接并建立通信的引脚/信号。

    .. include:: {IDF_TARGET_TOOLCHAIN_NAME}.inc
        :start-after: jtag-pins
        :end-before: ---

2.  检查 {IDF_TARGET_NAME} 上用于 JTAG 通信的的引脚是否被连接到了其它硬件上，这可能会影响 JTAG 的工作。

3.  连接 {IDF_TARGET_NAME} 和 JTAG 接口上的引脚/信号。


配置驱动
^^^^^^^^
你可能还需要安装软件驱动，才能使 JTAG 在计算机上正常工作，请参阅你所使用的 JTAG 适配器的有关文档，获取相关详细信息。


连接
^^^^

将 JTAG 接口连接到计算机，打开 {IDF_TARGET_NAME} 和 JTAG 接口板上的电源，然后检查计算机是否可以识别到 JTAG 接口。


要继续设置调试环境，请前往 :ref:`jtag-debugging-run-openocd` 章节。

