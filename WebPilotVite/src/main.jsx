import React from 'react';
import { createRoot } from 'react-dom/client';

// Mismos estilos globales que el layout de Next (app/layout.jsx): tokens de
// la familia compartida, widgets y CSS local de la página.
import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import '../WebPilot/app/globals.css';
import './src/main.css';

import PilotPage from '../WebPilot/app/page.jsx';

createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <PilotPage />
  </React.StrictMode>,
);
