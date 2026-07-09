@echo off
chcp 65001 >nul
set "output=all_source_full.md"
:: 清空旧文件
echo. > "%output%"

echo # RobotEyes v6.1 完整源码汇总 >> "%output%"
echo 导出时间：%date% %time% >> "%output%"
echo 包含文件：*.h + *.cpp >> "%output%"
echo. >> "%output%"

echo ## 完整目录树 >> "%output%"
echo ```tree >> "%output%"
tree /f /a >> "%output%"
echo ``` >> "%output%"
echo. >> "%output%"

:: 递归遍历 .h .cpp 并打印进度
for /r %%f in (*.h *.cpp) do (
    echo 正在导出：%%f
    echo ## 文件路径：%%f >> "%output%"
    echo ```c >> "%output%"
    type "%%f" >> "%output%"
    echo ``` >> "%output%"
    echo. >> "%output%"
    echo --- >> "%output%"
    echo. >> "%output%"
)

echo 导出完成！完整文件：%cd%\%output%
pause >nul