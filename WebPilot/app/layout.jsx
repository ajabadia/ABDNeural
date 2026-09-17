import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import './globals.css';

export const metadata = {
  title: 'NEURONiK Web Pilot',
  description: 'Minimal static WebView2 UI pilot for NEURONiK',
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
