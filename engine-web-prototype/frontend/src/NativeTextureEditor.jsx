import React, {useEffect, useRef, useState} from 'react';
import {Icon} from './StudioParts.jsx';
import {ApplyTextureChannel} from './texturePreview.js';
import './nativeTextureEditor.css';

export default function NativeTextureEditor({snapshot: Snapshot, asset: Asset, focusRequest: FocusRequest, error: ErrorMessage}) {
  const [Channel, SetChannel] = useState('RGBA');
  const [Background, SetBackground] = useState('checker');
  const [Zoom, SetZoom] = useState(1);
  const [Fit, SetFit] = useState(true);
  const [ImageError, SetImageError] = useState('');
  const Canvas = useRef(null);
  const Surface = useRef(null);
  const Drag = useRef(null);
  const Metadata = Snapshot.document;
  useEffect(() => {SetFit(true);}, [FocusRequest]);
  useEffect(() => {
    function FitImage() {
      if (Fit && Metadata.width && Surface.current) {
        SetZoom(Math.min(1, Math.max(0.01, Math.min((Surface.current.clientWidth - 48) / Metadata.width, (Surface.current.clientHeight - 48) / Metadata.height))));
      }
    }
    FitImage();
    const Observer = new ResizeObserver(FitImage);
    Observer.observe(Surface.current);
    return () => Observer.disconnect();
  }, [Fit, Metadata.width, Metadata.height]);
  useEffect(() => {
    if (!Snapshot.previewPng) {
      return;
    }
    let Disposed = false;
    const PreviewImage = new Image();
    PreviewImage.onload = () => {
      if (Disposed || !Canvas.current) {
        return;
      }
      Canvas.current.width = PreviewImage.width;
      Canvas.current.height = PreviewImage.height;
      const Context = Canvas.current.getContext('2d');
      Context.drawImage(PreviewImage, 0, 0);
      const ImagePixels = Context.getImageData(0, 0, PreviewImage.width, PreviewImage.height);
      ApplyTextureChannel(ImagePixels.data, Channel);
      Context.putImageData(ImagePixels, 0, 0);
      SetImageError('');
    };
    PreviewImage.onerror = () => {
      if (!Disposed) {
        SetImageError('Не удалось отобразить предпросмотр текстуры.');
      }
    };
    PreviewImage.src = 'data:image/png;base64,' + Snapshot.previewPng;
    return () => {Disposed = true;};
  }, [Snapshot.previewPng, Channel]);
  function ChangeZoom(Value) {
    SetFit(false);
    SetZoom(Math.min(8, Math.max(0.01, Value)));
  }
  const Failure = ErrorMessage || Snapshot.diagnostic || ImageError;
  let SourceChannels = '—';
  if (Metadata.channelCount) {
    SourceChannels = String(Metadata.channelCount);
  }
  let DecodedSize = '—';
  if (Metadata.decodedBytes) {
    DecodedSize = (Metadata.decodedBytes / 1048576).toFixed(2) + ' МиБ';
  }
  return <main className="native-texture-editor" aria-label="Редактор текстуры">
    <header className="native-texture-toolbar">
      <Icon name="Image"/><strong title={Asset?.path}>{Asset?.name || 'Текстура'}</strong>
      <button onClick={() => window.sacuraDesktop.nativeRevealAsset({assetId: Snapshot.assetId})}>Показать в Content Browser</button>
    </header>
    <div className="native-texture-body">
      <section className="native-texture-preview" aria-label="Предпросмотр текстуры">
        <nav aria-label="Настройки просмотра">
          <label>Каналы<select aria-label="Каналы" value={Channel} onChange={Event => SetChannel(Event.target.value)}>{['RGBA', 'RGB', 'R', 'G', 'B', 'A'].map(Value => <option key={Value}>{Value}</option>)}</select></label>
          <label>Фон<select aria-label="Фон" value={Background} onChange={Event => SetBackground(Event.target.value)}><option value="checker">Шахматный</option><option value="dark">Тёмный</option><option value="light">Светлый</option></select></label>
          <button aria-pressed={Fit} onClick={() => SetFit(true)}>Вписать</button>
          <button aria-pressed={!Fit && Zoom === 1} onClick={() => ChangeZoom(1)}>1:1</button>
          <label>Масштаб<input aria-label="Масштаб, %" type="number" min="1" max="800" value={Math.round(Zoom * 100)} onChange={Event => {
            const Value = Number(Event.target.value);
            if (Number.isFinite(Value) && Value > 0) {
              ChangeZoom(Value / 100);
            }
          }}/>%</label>
        </nav>
        <div ref={Surface} className={'native-texture-surface ' + Background} tabIndex={0} aria-label="Область изображения" onKeyDown={Event => {
          if (Event.code === 'KeyF') {
            SetFit(true);
          }
        }} onPointerDown={Event => {
          if (Event.button === 0) {
            Drag.current = {PositionX: Event.clientX, PositionY: Event.clientY, Left: Event.currentTarget.scrollLeft, Top: Event.currentTarget.scrollTop};
            Event.currentTarget.setPointerCapture(Event.pointerId);
          }
        }} onPointerMove={Event => {
          if (Drag.current) {
            Event.currentTarget.scrollLeft = Drag.current.Left - Event.clientX + Drag.current.PositionX;
            Event.currentTarget.scrollTop = Drag.current.Top - Event.clientY + Drag.current.PositionY;
          }
        }} onPointerUp={Event => {
          Drag.current = null;
          if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
            Event.currentTarget.releasePointerCapture(Event.pointerId);
          }
        }} onLostPointerCapture={() => {Drag.current = null;}}>
          {Snapshot.preparing && <p role="status">Загрузка текстуры…</p>}
          {Failure && <p role="alert">{Failure}</p>}
          {!Snapshot.preparing && !Failure && Snapshot.previewPng && <div className="native-texture-image-area"><canvas ref={Canvas} role="img" aria-label={Asset?.name || 'Текстура'} style={{width: Metadata.width * Zoom, height: Metadata.height * Zoom}}/></div>}
        </div>
      </section>
      <aside className="native-texture-details" aria-label="Свойства текстуры">
        <h3>Свойства текстуры</h3>
        <details open><summary>Исходник</summary><dl><dt>Размер</dt><dd>{Metadata.width || '—'} × {Metadata.height || '—'}</dd><dt>Каналы декодирования</dt><dd>{SourceChannels}</dd><dt>Формат декодирования</dt><dd>{Metadata.format || '—'}</dd><dt>Цветовое пространство</dt><dd>{Metadata.colorSpace || '—'}</dd><dt>Декодировано в памяти</dt><dd>{DecodedSize}</dd></dl><p className="native-texture-path">{Asset?.path}</p></details>
        <details open><summary>Компрессия</summary><p>Сжатие текстур пока не поддерживается загрузчиком. Параметры компрессии будут доступны после добавления обработки текстур.</p></details>
        <details open><summary>Уровни детализации</summary><p>Выбор mip-уровня недоступен: текущий загрузчик предоставляет исходное изображение RGBA8.</p></details>
        <details><summary>Просмотр</summary><p>Каналы, фон и масштаб изменяют только предпросмотр. Перетаскивание перемещает увеличенное изображение. F вписывает текстуру в окно.</p></details>
      </aside>
    </div>
    <footer><span>{Asset?.path}</span><span>Просмотр исходника</span>{Metadata.previewWidth && Metadata.previewWidth !== Metadata.width && <span>Предпросмотр: {Metadata.previewWidth} × {Metadata.previewHeight}</span>}</footer>
  </main>;
}
