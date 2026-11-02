#pragma once
namespace smk::p4 {
bool beginDisplayTest(bool diagnostic_controls = true);
void serviceDisplayTest();
void setLinkStatus(const char* text);
}
