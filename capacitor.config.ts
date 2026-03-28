import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.overheadtracker.app',
  appName: 'Overhead Tracker',
  webDir: 'www',
  server: {
    allowNavigation: ['api.overheadtracker.com']
  }
};

export default config;
