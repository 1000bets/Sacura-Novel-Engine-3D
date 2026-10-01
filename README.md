# Sacura Novel Engine 3D

Репозиторий движка для 3D-визуальных новелл и point-and-click историй. Будущий C++-движок пока не реализован.

Веб-прототип редактора на React и Three.js, его сервер, тесты, контейнеры и инфраструктура находятся в [engine-web-prototype](engine-web-prototype/README.md).

Для запуска редактора:

```sh
cd engine-web-prototype/frontend
npm install
npm run dev
```

Контейнерный запуск с SQLite и S3, настройка HTTPS и резервное копирование описаны в [руководстве размещения](engine-web-prototype/deploy/README.md). Команды Docker Compose выполняются из `engine-web-prototype`.
