/*
Copyright (C) 2023- YAYC team <info@yayc.stream>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training, fine-tuning, or otherwise feeding artificial intelligence systems is prohibited for both commercial and non-commercial use.
  This includes, but is not limited to, the ingestion of this work into large language models (LLMs), code generation models,
  Retrieval-Augmented Generation (RAG) systems, embedding databases, vector stores, or any other AI-assisted system.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

#include "SiteQuirks.h"

namespace {

// qutebrowser's value; os_info as Firefox writes it.
#if defined(Q_OS_WIN)
#define YAYC_FIREFOX_OS "Windows NT 10.0; Win64; x64"
#elif defined(Q_OS_MACOS)
#define YAYC_FIREFOX_OS "Macintosh; Intel Mac OS X 10.15"
#else
#define YAYC_FIREFOX_OS "X11; Linux x86_64"
#endif

const QByteArray firefoxUserAgent =
    QByteArrayLiteral("Mozilla/5.0 (" YAYC_FIREFOX_OS "; rv:149.0) Gecko/20100101 Firefox/149.0");

} // namespace

void SiteQuirksInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    const QUrl url = info.requestUrl();
    if (url.scheme() == QLatin1String("https")
            && url.host() == QLatin1String("accounts.google.com"))
        info.setHttpHeader(QByteArrayLiteral("User-Agent"), firefoxUserAgent);
}
