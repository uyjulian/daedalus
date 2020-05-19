#pragma once

#ifndef SYSTEM_COND_H_
#define SYSTEM_COND_H_

class Mutex;
struct Cond;

extern const double kTimeoutInfinity;

Cond * CondCreate();
void CondDestroy(Cond * cond);
void CondWait(Cond * cond, Mutex * mutex, double timeout);
void CondSignal(Cond * cond);

#endif // SYSTEM_COND_H_
