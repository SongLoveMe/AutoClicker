#ifndef WORKFLOWSTORE_H
#define WORKFLOWSTORE_H

#include <QString>
#include "Workflow.h"

class WorkflowStore
{
public:
    static bool save(const Workflow& workflow, const QString& path, QString* error = nullptr);
    static bool load(const QString& path, Workflow* workflow, QString* error = nullptr);
    static bool loadLegacyConfig(const QString& path, Workflow* workflow, QString* error = nullptr);
    static QString safeFileName(const QString& name);
};

#endif
