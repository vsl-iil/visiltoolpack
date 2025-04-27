Пакет утилит, полезных при анализе ВПО

cpuspike
--------

Логирует скачки использования процессора и процессы, которые во время скачка
используют больше всего процессорного времени. Позволяет настраивать искомую 
величину скачка, длительность и задержку перед началом измерения.

```
cpuspike v.0.9.0  Copyright (C) 2025  Egor Lvov
Usage:
  cpuspike [OPTION...]
        -h | --help                 -  display this help
        -d | --duration <SECONDS>   -  how long should the spike be to be detected (default: 1)
        -t | --threshold <PERCENT>  -  how high should the spike be to be detected (default: 50%)
        -i | --idle <SECONDS>       -  wait SECONDS of user idling before starting (default: 30)
```

*TODO: сделать задержку от момента последней активности пользователя, добавить относительный детект скачков.*


dllinject
---------

Миниатюрная утилита для инъекции DLL в процессы.

```
Usage: dllinject.exe <PROCESS NAME> <DLL PATH>
```


exclumon
--------

Мониторинг исключений Windows Defender. При добавлении/удалении исключений 
отправляет уведомление.

*TODO: работа в режиме сервиса*
