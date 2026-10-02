/*
 * ============================================================
 * file_system_panel.h
 * ============================================================
 * يعرض شجرة ملفات ومجلدات حقيقية من القرص - حصراً مجلد Assets/
 * الخاص بالمشروع المفتوح حالياً (current_project.h)، بلا أي وصول
 * لأي مكان آخر بجهاز المستخدم.
 * ============================================================
 */

#ifndef FILE_SYSTEM_PANEL_H
#define FILE_SYSTEM_PANEL_H

void file_system_panel_update(int x, int y, int w, int h);
void file_system_panel_draw(int x, int y, int w, int h);
void file_system_panel_shutdown(void);

#endif /* FILE_SYSTEM_PANEL_H */
