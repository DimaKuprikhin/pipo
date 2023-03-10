# Мойка посуды

## Условие
На мойке посуды в ресторане работают два человека. Один из них моет посуду, второй вытирает уже вымытую. Времена выполнения операций мытья и вытирания посуды меняются в зависимости от того, что моется. Стол для вымытой, но не вытертой посуды имеет ограниченные размеры. Смоделируйте процесс работы персонала следующим образом: каждому работнику соответствует свой процесс. Времена выполнения операций содержатся в двух файлах. Каждый файл имеет формат записей: 
<тип посуды> : <время операции>
Стол вмещает N предметов независимо от их наименования. Значение N задается как параметр среды TABLE_LIMIT перед стартом процессов. Грязная посуда, поступающая на мойку, описывается файлом с форматом записи: 
<тип посуды> : <количество предметов>
Записи с одинаковым типом посуды могут встречаться неоднократно. Организуйте передачу посуды от процесса к процессу: 
- через fifo, используя семафоры для синхронизации;
- через pipe;
- через сообщения;
- через разделяемую память;
- через sockets

## Пояснения к решению
Мойщик(Washer) выполняет работу в главном процессе, вытиратель(Wiper) работает в дочернем процессе. Вытиратель берет вымытую посуду со стола в том же порядке, в котором она была поставлена на стол. Каждое действие работников выводится в лог, с указанием работника и времени от старта программы.

Программа принимает аргументы с путями к файлам с данными и типом работников в таком порядке: времена мытья посуды, времена вытирания, список посуды для мытья и вытирания, тпи работников (fifo, pipe, shm, socket, msg).

Компиляция исполняемого файла с решением задачи выполняется командой `make task4`. Команда `make task4-test` выполнит решение с тестовыми данными для всех типов работников и сравнит лог программы с ожидаемым.

Программа логирует выполняемые действия работников (все действия происходят через `N` секунд после старта программы - время перед запуском работников и остальные побочные действия не учитываются):
- `WASHER N sec: Wash DISH for M seconds` - мойщик начинает мыть посуду типа `DISH`, для чего ему потребуется `M` секунд. В течении этого времени мойщик не будет производить никаких других операций.
- `WASHER N sec: Trying to put DISH on the table` - мойщик пытается поставить вымытую посуду типа `DISH` на стол. Если на столе есть свободные места, мойщик поставит посуду на стол и сразу (в эту же секунду с момента старта программы) начнет мыть следующую посуду, если она есть. Иначе, мойщик дождется, пока вытиратель не возьмет какую-нибудь посуду со стола, и только тогда поставит вымытую посуду на освободившееся место.
- `WASHER N sec: Put DISH on the table` - мойщик поставил вымытую посуду типа `DISH` на стол.
- `WIPER  N sec: Trying to get dish from the table` - вытиратель пытается взять вымытую посуду со стола. Если на столе есть посуда, вытиратель сразу получает ту, которая была поставлена на стол первой. Иначе, процесс вытирателя блокируется до тех пор, пока мойщик не поставит на стол посуду.
- `WIPER  N sec: Got DISH from the table` - вытиратель взял посуду типа `DISH` со стола.
- `WIPER  N sec: Wipe DISH for M seconds` - вытиратель начинает вытирать посуду типа `DISH`, что потребует у него `M` секунд.
- `WORKER N sec: Finished work` - работник (`WASHER` или `WIPER`) завершил работу. Вытиратель обязательно заканчивает свою работу как минимум через 1 секунду после завершения работы мойщика.
