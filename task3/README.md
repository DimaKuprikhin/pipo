# USELESS

## Условие
Напишите программу runsim, осуществляющую контроль количества одновременно работающих UNIX-приложений. Программа читает UNIX-команду со стандартного ввода и запускает ее на выполнение. Количество одновременно работающих команд не должно превышать N, где N – параметр командной строки при запуске runsim. При попытке запустить более чем N приложений выдайте сообщение об ошибке. Программа runsim должна прекращать свою работу при возникновении признака конца файла на стандартном вводе.

## Пояснения к решению
Компиляция исполняемого файла решения выполняется командой `make task3`.

Пример взаимодействия с программой:
```bash
dima@dima-Lenovo:~/projects/pipo$ ./task3/runsim 3
/usr/bin/sleep 20
Run /usr/bin/sleep in process 0
/usr/bin/sleep 4
Run /usr/bin/sleep in process 1
/usr/bin/sleep 10
Run /usr/bin/sleep in process 2
/usr/bin/sleep 2
You reached the limit of programs running simultaneously
/usr/bin/sleep 20
Run /usr/bin/sleep in process 1
/usr/bin/ls
You reached the limit of programs running simultaneously
/usr/bin/ls
Run /usr/bin/ls in process 2
Makefile  README.md  task1  task3  task4  task5  task6
```
