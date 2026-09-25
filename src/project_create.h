/*
 * ============================================================
 * project_create.h
 * ============================================================
 * ينشئ هيكل مشروع جديد فعلياً على القرص: مجلد باسم المشروع داخل
 * المسار المحدد، مجلد Assets/ فارغ بداخله، وملف إعدادات المشروع
 * project.rebax. ينشئ أي مجلد وسيط ناقص تلقائياً لو غير موجود.
 * ============================================================
 */

#ifndef PROJECT_CREATE_H
#define PROJECT_CREATE_H

typedef enum {
    PROJECT_CREATE_OK = 0,
    PROJECT_CREATE_ERROR_INVALID_INPUT, /* اسم أو مسار فارغ */
    PROJECT_CREATE_ERROR_MKDIR,         /* فشل إنشاء مجلد (صلاحيات، إلخ) */
    PROJECT_CREATE_ERROR_FILE           /* فشل كتابة project.rebax */
} project_create_result_t;

/* ينشئ مجلد المشروع كاملاً: <base_path>/<project_name>/ يحتوي على
 * Assets/ (فارغ) و project.rebax (إعدادات أولية). */
project_create_result_t project_create(const char *project_name, const char *base_path);

#endif /* PROJECT_CREATE_H */
