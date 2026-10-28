# ZKgram

ZKgram — неофициальный клиент Telegram для Windows (ПК), собранный на основе
[AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop), с перенесёнными
функциями из [Nagram X](https://github.com/risin42/NagramX).

## Откуда взят код и почему это законно

| Проект | Лицензия | Что взято |
|---|---|---|
| [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) | GPL-3.0 (+ OpenSSL exception) | основа клиента |
| [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) | GPL-3.0 | весь исходный код (режим призрака, сохранение удалённых сообщений, фильтры и т.д.) |
| [Nagram X](https://github.com/risin42/NagramX) | GPL-3.0 | идеи и логика функций, переписанные на C++ для десктопа |

Все три проекта распространяются под GNU GPL v3, которая разрешает копировать,
изменять и распространять код при условии, что производная работа тоже остаётся
под GPL-3.0, сохраняются уведомления об авторских правах и (при распространении)
доступен исходный код. Поэтому:

* файл [LICENSE](LICENSE) и [LEGAL](LEGAL) сохранены без изменений;
* заголовки с авторством в исходных файлах не удалялись;
* у приложения своё название, иконка и идентификатор — товарные знаки
  «AyuGram», «Nagram», «Telegram» и их логотипы не используются как бренд ZKgram.

ZKgram не связан с Telegram, Radolyn Labs (AyuGram) или авторами Nagram X.

## Сборка

Сборка выполняется автоматически в GitHub Actions
(`.github/workflows/win.yml`), готовый `ZKgram.exe` появляется во вкладке
**Actions → Windows build → Artifacts**.

Перед сборкой добавь в **Settings → Secrets and variables → Actions** свои
ключи Telegram API (получить на https://my.telegram.org → API development tools):

* `TG_API_ID`
* `TG_API_HASH`

Если секретов нет, используются публичные ключи официального Telegram Desktop
(как в AyuGram, см. `docs/api_credentials.md`). Это нарушает правила Telegram
API, поэтому по возможности заведи свои ключи.

Подробнее: [docs/zkgram.md](docs/zkgram.md).

## Предупреждение

Функции вроде режима призрака или сохранения удалённых сообщений нарушают
[условия использования Telegram API](https://core.telegram.org/api/terms).
Telegram может ограничить аккаунт, который пользуется неофициальным клиентом.
Используй на свой риск.
