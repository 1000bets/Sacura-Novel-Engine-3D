#pragma once
#include <QStringList>
// Returns -1 for normal viewport startup. No JavaScript document serializer.
int RunProjectCli(const QStringList& Arguments);
