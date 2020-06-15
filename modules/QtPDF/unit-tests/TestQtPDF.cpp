/*
  This is part of TeXworks, an environment for working with TeX documents
  Copyright (C) 2013-2020  Stefan Löffler

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For links to further information, or to contact the authors,
  see <http://www.tug.org/texworks/>.
*/
#include "TestQtPDF.h"
#include "PaperSizes.h"

#ifdef USE_MUPDF
  typedef QtPDF::MuPDFBackend Backend;
#elif USE_POPPLERQT
  typedef QtPDF::PopplerQtBackend Backend;
#else
  #error Must specify one backend
#endif

namespace QtPDF {
void compareDestination(const QtPDF::PDFDestination & a, const QtPDF::PDFDestination & b)
{
  QCOMPARE(a.isExplicit(), b.isExplicit());
  QCOMPARE(a.page(), b.page());
  if (a.isExplicit()) {
    QCOMPARE(a.rect(), b.rect());
    QCOMPARE(a.zoom(), b.zoom());
  }
  else
    QCOMPARE(a.destinationName(), b.destinationName());
}

bool operator== (const QtPDF::PDFAction & a, const QtPDF::PDFAction & b) {
  if (a.type() != b.type()) return false;
  switch (a.type()) {
  case QtPDF::PDFAction::ActionTypeGoTo:
  {
    const QtPDF::PDFGotoAction & A = reinterpret_cast<const QtPDF::PDFGotoAction &>(a);
    const QtPDF::PDFGotoAction & B = reinterpret_cast<const QtPDF::PDFGotoAction &>(b);

    compareDestination(A.destination(), B.destination());
    if (QTest::currentTestFailed()) return false;
    if (A.isRemote() != B.isRemote()) return false;
    if (A.filename() != B.filename()) return false;
    if (A.openInNewWindow() != B.openInNewWindow()) return false;

    return true;
  }
  case QtPDF::PDFAction::ActionTypeURI:
  {
    const QtPDF::PDFURIAction & A = reinterpret_cast<const QtPDF::PDFURIAction &>(a);
    const QtPDF::PDFURIAction & B = reinterpret_cast<const QtPDF::PDFURIAction &>(b);
    return A.url() == B.url();
  }
  default:
    return false;
  }
}
} // namespace QtPDF

namespace UnitTest {

class ComparableImage : public QImage {
  double _threshold;
public:
  explicit ComparableImage(const QImage & other, const double threshold = 3) : QImage(other.convertToFormat(QImage::Format_RGB32)), _threshold(threshold) { }
  explicit ComparableImage(const QString & filename, const double threshold = 3) : QImage(QImage(filename).convertToFormat(QImage::Format_RGB32)), _threshold(threshold) { }

  bool operator==(const ComparableImage & other) const {
    Q_ASSERT(format() == QImage::Format_RGB32);
    Q_ASSERT(other.format() == QImage::Format_RGB32);

    double threshold = qMax(_threshold, other._threshold);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (byteCount() != other.byteCount()) return false;
#else
    if (sizeInBytes() != other.sizeInBytes()) return false;
#endif

    double diff = 0.0;
    const uchar * src = bits();
    const uchar * dst = other.bits();
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    for (int i = 0; i < byteCount(); ++i)
      diff += qAbs(static_cast<int>(src[i]) - static_cast<int>(dst[i]));
#else
    for (qsizetype i = 0; i < sizeInBytes(); ++i)
      diff += qAbs(static_cast<int>(src[i]) - static_cast<int>(dst[i]));
#endif

    diff /= size().width() * size().height();

    if (diff >= threshold)
      qDebug() << "Difference" << diff << ">=" << threshold;

    return diff < threshold;
  }
};

inline void sleep(quint64 ms)
{
#ifdef Q_OS_MACOS
  // QTest::qSleep seems very unreliable on Mac OS X (QTBUG-84998)
  QElapsedTimer t;
  t.start();
  qint64 dt = (ms < 100 ? 1 : ms / 100);
  while(t.elapsed() < ms) {
    QTest::qSleep(dt);
  }
  // Issue a warning if the timing is off by more than 20%
  if (t.elapsed() > 1.2 * ms) {
    qWarning() << t.elapsed() << "ms have passed instead of the requested" << ms << "ms";
  }
#else
  QTest::qSleep(ms);
#endif
}

QTestData & TestQtPDF::newDocTest(const char * tag)
{
  return QTest::newRow(tag) << _docs[QString::fromUtf8(tag)];
}

QTestData & TestQtPDF::newPageTest(const char * tag, const unsigned int iPage)
{
  return QTest::newRow(qPrintable(QString::fromUtf8(tag) + QString::fromLatin1(" p") + QString::number(iPage + 1))) << _docs[QString::fromUtf8(tag)]->page(iPage).toStrongRef();
}

void TestQtPDF::backendInterface()
{
#ifdef USE_MUPDF
  QtPDF::MuPDFBackend backend;
  QCOMPARE(backend.name(), QString::fromLatin1("mupdf"));
#elif USE_POPPLERQT
  QtPDF::PopplerQtBackend backend;
  QCOMPARE(backend.name(), QString::fromLatin1("poppler-qt"));
#else
  #error Must specify one backend
#endif

  QVERIFY(backend.canHandleFile(QString::fromLatin1("test.pdf")));
  QVERIFY(!backend.canHandleFile(QString::fromLatin1("test.tex")));
}

void TestQtPDF::loadDocs()
{
  Backend backend;

  // Don't run documents that may produce error messages in QBENCHMARK as
  // otherwise the console may be filled with unhelpful messages
  _docs[QString::fromLatin1("invalid")] = backend.newDocument(QString());
  _docs[QString::fromLatin1("base14-locked")] = backend.newDocument(QString::fromLatin1("base14-fonts-locked.pdf"));

  QBENCHMARK {
    _docs[QString::fromLatin1("transitions")] = backend.newDocument(QString::fromLatin1("pdf-transitions.pdf"));
    _docs[QString::fromLatin1("pgfmanual")] = backend.newDocument(QString::fromLatin1("pgfmanual.pdf"));
    _docs[QString::fromLatin1("base14-fonts")] = backend.newDocument(QString::fromLatin1("base14-fonts.pdf"));
    _docs[QString::fromLatin1("poppler-data")] = backend.newDocument(QString::fromLatin1("poppler-data.pdf"));
    _docs[QString::fromLatin1("metadata")] = backend.newDocument(QString::fromLatin1("metadata.pdf"));
    _docs[QString::fromLatin1("page-rotation")] = backend.newDocument(QString::fromLatin1("page-rotation.pdf"));
    _docs[QString::fromLatin1("annotations")] = backend.newDocument(QString::fromLatin1("annotations.pdf"));
  }
}

void TestQtPDF::isValid_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<bool>("expected");
  newDocTest("invalid") << false;
  newDocTest("transitions") << true;
  newDocTest("pgfmanual") << true;
  newDocTest("base14-fonts") << true;
  newDocTest("base14-locked") << true;
  newDocTest("metadata") << true;
  newDocTest("page-rotation") << true;
}

void TestQtPDF::isValid()
{
  QFETCH(pDoc, doc);
  QFETCH(bool, expected);
  QCOMPARE(doc->isValid(), expected);
}

void TestQtPDF::isLocked_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<bool>("expected");
  newDocTest("invalid") << false;
  newDocTest("transitions") << false;
  newDocTest("pgfmanual") << false;
  newDocTest("base14-fonts") << false;
  newDocTest("base14-locked") << true;
  newDocTest("metadata") << false;
  newDocTest("page-rotation") << false;
}

void TestQtPDF::isLocked()
{
  QFETCH(pDoc, doc);
  QFETCH(bool, expected);
  QCOMPARE(doc->isLocked(), expected);
}

void TestQtPDF::unlock_data()
{
  // For testing unlock(), we must freshly load the pdf files each time to
  // ensure that a previously successful unlock does not interfer with a later
  // unlock attempt. Also, unlocking the cached "base14-locked" document would
  // cause subsequent test cases (which assume it is locked) to fail.
  QTest::addColumn<QString>("filename");
  QTest::addColumn<QString>("pwd");
  QTest::addColumn<bool>("success");

  QTest::newRow("unlocked") << QString::fromLatin1("base14-fonts.pdf") << QString() << true;
  QTest::newRow("wrong-pwd") << QString::fromLatin1("base14-fonts-locked.pdf") << QString() << false;
  QTest::newRow("user-pwd") << QString::fromLatin1("base14-fonts-locked.pdf") << QString::fromLatin1("123") << true;
  QTest::newRow("owner-pwd") << QString::fromLatin1("base14-fonts-locked.pdf") << QString::fromLatin1("test") << true;
}

void TestQtPDF::unlock()
{
  QFETCH(QString, filename);
  QFETCH(QString, pwd);
  QFETCH(bool, success);
  Backend backend;

  pDoc doc = backend.newDocument(filename);
  QCOMPARE(doc->unlock(pwd), success);
}

void TestQtPDF::numPages_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<int>("expected");
  newDocTest("invalid") << -1;
  newDocTest("transitions") << 29;
  newDocTest("pgfmanual") << 726;
  newDocTest("base14-fonts") << 1;
  newDocTest("base14-locked") << 1;
  newDocTest("metadata") << 1;
  newDocTest("page-rotation") << 4;
}

void TestQtPDF::numPages()
{
  QFETCH(pDoc, doc);
  QFETCH(int, expected);
#ifdef USE_POPPLERQT
  QEXPECT_FAIL("base14-locked", "poppler-qt doesn't report page numbers for locked documents", Continue);
#endif
  QCOMPARE(doc->numPages(), expected);
}

void TestQtPDF::fileName_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString::fromLatin1("pdf-transitions.pdf");
  newDocTest("pgfmanual") << QString::fromLatin1("pgfmanual.pdf");
  newDocTest("base14-fonts") << QString::fromLatin1("base14-fonts.pdf");
  newDocTest("base14-locked") << QString::fromLatin1("base14-fonts-locked.pdf");
  newDocTest("metadata") << QString::fromLatin1("metadata.pdf");
  newDocTest("page-rotation") << QString::fromLatin1("page-rotation.pdf");
}

void TestQtPDF::fileName()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->fileName(), expected);
}

void TestQtPDF::page_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QVariant>("pageSize");

  newDocTest("invalid") << QVariant(QSizeF());
  newDocTest("transitions") << QVariant(QSizeF(362.835, 272.126));
  newDocTest("pgfmanual") << QVariant(QSizeF(595.276, 841.89));
  newDocTest("base14-fonts") << QVariant(QSizeF(595, 842));
  newDocTest("base14-locked") << QVariant(QSizeF(595, 842));
  newDocTest("metadata") << QVariant(QSizeF(612, 792));
  newDocTest("page-rotation") << QVariant::fromValue(QVariantList() << QVariant(QSizeF(595, 842)) << QVariant(QSizeF(842, 595)) << QVariant(QSizeF(595, 842)) << QVariant(QSizeF(842, 595)));
}

void TestQtPDF::page()
{
  QFETCH(pDoc, doc);
  QFETCH(QVariant, pageSize);

  QList<QSizeF> pageSizes;

  QVERIFY(!doc.isNull());
  QVERIFY(doc->page(-1).isNull());
  QVERIFY(doc->page(doc->numPages()).isNull());

  if (pageSize.type() == QVariant::SizeF) {
    for (int i = 0; i < doc->numPages(); ++i)
      pageSizes.append(pageSize.toSizeF());
  }
  else if (pageSize.type() == QVariant::List) {
    QVariantList l(pageSize.value<QVariantList>());
    while (pageSizes.length() < doc->numPages()) {
      for (int i = 0; i < l.length(); ++i)
        pageSizes.append(l[i].toSizeF());
    }
  }
  else {
    QFAIL(pageSize.typeName());
  }

  for (int i = 0; i < doc->numPages(); ++i) {
    QSharedPointer<QtPDF::Backend::Page> page = doc->page(i).toStrongRef();
    QSizeF size = pageSizes[i];

    QVERIFY(!page.isNull());
    QVERIFY(page->pageNum() == i);
#ifdef USE_POPPLERQT
    QEXPECT_FAIL("base14-locked", "poppler-qt doesn't report page sizes for locked documents", Continue);
#endif
    QVERIFY2(qAbs(page->pageSizeF().width() - size.width()) < 1e-4, qPrintable(QString::fromLatin1("Width of page %1 is %2 instead of %3").arg(i + 1).arg(page->pageSizeF().width()).arg(size.width())));
    QVERIFY2(qAbs(page->pageSizeF().height() - size.height()) < 1e-4, qPrintable(QString::fromLatin1("Height of page %1 is %2 instead of %3").arg(i + 1).arg(page->pageSizeF().height()).arg(size.height())));

//		transition
//		loadLinks()
//		boxes()
//		selectedText
//		renderToImage
//		loadAnnotations
//		search
  }
}

void TestQtPDF::resolveDestination_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QtPDF::PDFDestination>("src");
  QTest::addColumn<QtPDF::PDFDestination>("dest");

  {
    QtPDF::PDFDestination d;
    d.setDestinationName(QString::fromLatin1("does not exist"));
    newDocTest("annotations") << d << QtPDF::PDFDestination();
  }
  {
    QtPDF::PDFDestination d(1);
    QtPDF::PDFDestination n(1);
    d.setRect(QRectF(102, 750.89, -1, -1));
    n.setDestinationName(QString::fromLatin1("page.2"));
    newDocTest("annotations") << d << d;
    newDocTest("annotations") << n << d;
  }
}

void TestQtPDF::resolveDestination()
{
  QFETCH(pDoc, doc);
  QFETCH(QtPDF::PDFDestination, src);
  QFETCH(QtPDF::PDFDestination, dest);

  QtPDF::PDFDestination actual = doc->resolveDestination(src);

  compareDestination(actual, dest);
}

void TestQtPDF::metaDataTitle_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString();
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromUtf8("Document Title • UTF16-BE");
}

void TestQtPDF::metaDataTitle()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->title(), expected);
}

void TestQtPDF::metaDataAuthor_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString();
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromUtf8("Stefan Löffler");
}

void TestQtPDF::metaDataAuthor()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->author(), expected);
}

void TestQtPDF::metaDataSubject_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString();
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromLatin1("PDF Test File");
}

void TestQtPDF::metaDataSubject()
{
    QFETCH(pDoc, doc);
    QFETCH(QString, expected);
    QCOMPARE(doc->subject(), expected);
}

void TestQtPDF::metaDataKeywords_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString();
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromLatin1("pdf, metadata, test");
}

void TestQtPDF::metaDataKeywords()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->keywords(), expected);
}

void TestQtPDF::metaDataCreator_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString::fromUtf8("LaTeX with hyperref package");
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromLatin1("gedit");
}

void TestQtPDF::metaDataCreator()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->creator(), expected);
}

void TestQtPDF::metaDataProducer_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QString>("expected");
  newDocTest("invalid") << QString();
  newDocTest("transitions") << QString();
  newDocTest("pgfmanual") << QString::fromUtf8("pdfTeX-1.40.10");
  newDocTest("base14-fonts") << QString();
  newDocTest("base14-locked") << QString();
  newDocTest("metadata") << QString::fromLatin1("also gedit");
}

void TestQtPDF::metaDataProducer()
{
  QFETCH(pDoc, doc);
  QFETCH(QString, expected);
  QCOMPARE(doc->producer(), expected);
}

void TestQtPDF::metaDataCreationDate_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QDateTime>("expected");
  newDocTest("invalid") << QDateTime();
  newDocTest("transitions") << QDateTime();
  newDocTest("pgfmanual") << QDateTime(QDate(2010, 10, 25), QTime(20, 56, 26), Qt::UTC);
  newDocTest("base14-fonts") << QDateTime();
  newDocTest("base14-locked") << QDateTime();
  newDocTest("metadata") << QDateTime(QDate(2013, 9, 7), QTime(23, 23, 45), Qt::UTC);
}

void TestQtPDF::metaDataCreationDate()
{
  QFETCH(pDoc, doc);
  QFETCH(QDateTime, expected);
  QCOMPARE(doc->creationDate(), expected);
}

void TestQtPDF::metaDataModDate_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QDateTime>("expected");
  newDocTest("invalid") << QDateTime();
  newDocTest("transitions") << QDateTime();
  newDocTest("pgfmanual") << QDateTime(QDate(2010, 10, 25), QTime(20, 56, 26), Qt::UTC);
  newDocTest("base14-fonts") << QDateTime();
  newDocTest("base14-locked") << QDateTime();
  newDocTest("metadata") << QDateTime(QDate(2013, 9, 8), QTime(10, 34, 56), Qt::UTC);
}

void TestQtPDF::metaDataModDate()
{
  QFETCH(pDoc, doc);
  QFETCH(QDateTime, expected);
  QCOMPARE(doc->modDate(), expected);
}

void TestQtPDF::metaDataTrapped_data()
{
    QTest::addColumn<pDoc>("doc");
    QTest::addColumn<int>("expected");
    newDocTest("invalid") << (int)QtPDF::Backend::Document::Trapped_Unknown;
    newDocTest("transitions") << (int)QtPDF::Backend::Document::Trapped_Unknown;
    newDocTest("pgfmanual") << (int)QtPDF::Backend::Document::Trapped_False;
    newDocTest("base14-fonts") << (int)QtPDF::Backend::Document::Trapped_Unknown;
    newDocTest("base14-locked") << (int)QtPDF::Backend::Document::Trapped_Unknown;
    newDocTest("metadata") << (int)QtPDF::Backend::Document::Trapped_Unknown;
}

void TestQtPDF::metaDataTrapped()
{
    QFETCH(pDoc, doc);
    QFETCH(int, expected);
#ifdef USE_POPPLERQT
    QEXPECT_FAIL("pgfmanual", "poppler-qt doesn't handle trapping properly", Continue);
#endif
    QCOMPARE((int)doc->trapped(), expected);
}

void TestQtPDF::metaDataOther_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QStringMap>("data");

  newDocTest("invalid") << QStringMap();

  {
    QStringMap a;
    a.insert(QString::fromLatin1("PTEX.FullBanner"), QString::fromLatin1("This is LuaTeX, Version 1.07.0 (TeX Live 2018)"));
    newDocTest("annotations") << a;
  }
}

void TestQtPDF::metaDataOther()
{
  QFETCH(pDoc, doc);
  QFETCH(QStringMap, data);

  QCOMPARE(doc->metaDataOther(), data);
}

void TestQtPDF::fileSize_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<qint64>("filesize");

  newDocTest("invalid") << static_cast<qint64>(0);
  newDocTest("base14-fonts") << static_cast<qint64>(3800);
  newDocTest("base14-locked") << static_cast<qint64>(4292);
  newDocTest("pgfmanual") << static_cast<qint64>(5346838);
  newDocTest("annotations") << static_cast<qint64>(11817);
}

void TestQtPDF::fileSize()
{
  QFETCH(pDoc, doc);
  QFETCH(qint64, filesize);

  QCOMPARE(doc->fileSize(), filesize);
}

void TestQtPDF::pageSize_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QSizeF>("size");

  newDocTest("invalid") << QSizeF();
  newDocTest("base14-fonts") << QSizeF(595, 842);
  newDocTest("base14-locked") << QSizeF();
  newDocTest("pgfmanual") << QSizeF(595.276, 841.89);
  newDocTest("page-rotation") << QSizeF(595, 842);
  newDocTest("transitions") << QSizeF(362.835, 272.126);
}

void TestQtPDF::pageSize()
{
  QFETCH(pDoc, doc);
  QFETCH(QSizeF, size);

  QCOMPARE(doc->pageSize(), size);
}

void TestQtPDF::permissions_data()
{
  using namespace QtPDF::Backend;

  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<Document::Permissions>("permissions");

  newDocTest("invalid") << Document::Permissions();
  newDocTest("base14-locked") << Document::Permissions();
  newDocTest("base14-fonts") << Document::Permissions(Document::Permission_Annotate |
                                                      Document::Permission_Assemble |
                                                      Document::Permission_Change |
                                                      Document::Permission_Extract |
                                                      Document::Permission_ExtractForAccessibility |
                                                      Document::Permission_FillForm |
                                                      Document::Permission_Print |
                                                      Document::Permission_PrintHighRes);
}

void TestQtPDF::permissions()
{
  QFETCH(pDoc, doc);
  QFETCH(QtPDF::Backend::Document::Permissions, permissions);

  QCOMPARE(doc->permissions(), permissions);
}

void TestQtPDF::fonts_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QStringList>("fontNames");

  newDocTest("invalid") << QStringList();
  newDocTest("base14-locked") << QStringList();
  newDocTest("base14-fonts") << (QStringList() << QString::fromLatin1("Times-Roman")
                                               << QString::fromLatin1("Times-Bold")
                                               << QString::fromLatin1("Times-Italic")
                                               << QString::fromLatin1("Times-BoldItalic")
                                               << QString::fromLatin1("Helvetica")
                                               << QString::fromLatin1("Helvetica-Bold")
                                               << QString::fromLatin1("Helvetica-Oblique")
                                               << QString::fromLatin1("Helvetica-BoldOblique")
                                               << QString::fromLatin1("Courier")
                                               << QString::fromLatin1("Courier-Bold")
                                               << QString::fromLatin1("Courier-Oblique")
                                               << QString::fromLatin1("Courier-BoldOblique")
                                               << QString::fromLatin1("Symbol")
                                               << QString::fromLatin1("ZapfDingbats"));
  newDocTest("annotations") << (QStringList() << QString::fromLatin1("LMRoman12-Regular")
                                              << QString::fromLatin1("LMRoman12-Bold")
                                              << QString::fromLatin1("LMMono12-Regular"));
}

void TestQtPDF::fonts()
{
  QFETCH(pDoc, doc);
  QFETCH(QStringList, fontNames);

  QList< QtPDF::Backend::PDFFontInfo > fonts = doc->fonts();
  QStringList actualFontNames;

  for (int i = 0; i < fonts.size(); ++i)
    actualFontNames.append(fonts[i].descriptor().pureName());

  QCOMPARE(actualFontNames, fontNames);
}

// static
void TestQtPDF::compareToC(const QtPDF::Backend::PDFToC & actual, const QtPDF::Backend::PDFToC & expected)
{
  QCOMPARE(actual.size(), expected.size());
  if (QTest::currentTestFailed()) return;

  for (int i = 0; i < actual.size(); ++i) {
    QCOMPARE(actual[i].label(), expected[i].label());
    compareToC(actual[i].children(), expected[i].children());
  }
}

// static
void TestQtPDF::printToC(const QtPDF::Backend::PDFToC & toc, const QString & indent)
{
  for (int i = 0; i < toc.size(); ++i) {
    qDebug() << qPrintable(indent + toc[i].label());
    printToC(toc[i].children(), indent + QString::fromLatin1("  "));
  }
}

void TestQtPDF::toc_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<QtPDF::Backend::PDFToC>("toc");

  newDocTest("invalid") << QtPDF::Backend::PDFToC();

  {
    QtPDF::Backend::PDFToC a;
    a << QtPDF::Backend::PDFToCItem(QString::fromLatin1("A"));
    a << QtPDF::Backend::PDFToCItem(QString::fromLatin1("LaTeX"));
    a[0].children() << QtPDF::Backend::PDFToCItem(QString::fromUtf8("aä®€"));
    newDocTest("annotations") << a;
  }
}

void TestQtPDF::toc()
{
  QFETCH(pDoc, doc);
  QFETCH(QtPDF::Backend::PDFToC, toc);

  compareToC(doc->toc(), toc);

  if (QTest::currentTestFailed()) {
    qDebug() << "Actual:";
    printToC(doc->toc());
    qDebug() << "Expected:";
    printToC(toc);
  }
}


void TestQtPDF::page_renderToImage_data()
{
  QTest::addColumn<pDoc>("doc");
  QTest::addColumn<int>("iPage");
  QTest::addColumn<QString>("filename");
  QTest::addColumn<double>("threshold");
  // Use a higher threshold for the base14 test since the fonts are not
  // embedded. Consequently, system fonts are used as replacements, but they can
  // differ substantially between different systems.
  newDocTest("base14-fonts") << 0 << "base14-fonts-1.png" << 10.;
  newDocTest("poppler-data") << 0 << "poppler-data-1.png" << 3.;
}

void TestQtPDF::page_renderToImage()
{
  QFETCH(pDoc, doc);
  QFETCH(int, iPage);
  QFETCH(QString, filename);
  QFETCH(double, threshold);

  QSharedPointer<QtPDF::Backend::Page> page = doc->page(iPage).toStrongRef();
  QVERIFY(page);
  ComparableImage render(page->renderToImage(150, 150), threshold);
//  render.save(filename);
  ComparableImage ref(filename);
  QVERIFY(render == ref);
}

// static
void TestQtPDF::printAction(const QtPDF::PDFAction & a)
{
  switch (a.type()) {
  case QtPDF::PDFAction::ActionTypeGoTo:
  {
    const QtPDF::PDFGotoAction & A = reinterpret_cast<const QtPDF::PDFGotoAction &>(a);
    qDebug() << "   GotoAction" << A.filename()
             << "remote:" << A.isRemote()
             << "newWindow:" << A.openInNewWindow()
             << "page:" << A.destination().page()
             << "destName:" << A.destination().destinationName()
             << "rect:" << A.destination().rect()
             << "zoom:" << A.destination().zoom();
    break;
  }
  case QtPDF::PDFAction::ActionTypeURI:
  {
    const QtPDF::PDFURIAction & A = reinterpret_cast<const QtPDF::PDFURIAction &>(a);
    qDebug() << "   URIAction" << A.url();
    break;
  }
  default:
    qDebug() << "   Type:" << a.type();
  }
}

// static
void TestQtPDF::compareLinks(const QtPDF::Annotation::Link & actual, const QtPDF::Annotation::Link & expected)
{
  QCOMPARE(actual.quadPoints(), expected.quadPoints());
  QTEST_ASSERT(actual.actionOnActivation());
  QTEST_ASSERT(expected.actionOnActivation());
  QCOMPARE(*(actual.actionOnActivation()), *(expected.actionOnActivation()));

}

void TestQtPDF::page_loadLinks_data()
{
  QTest::addColumn<pPage>("page");
  QTest::addColumn< QList<QtPDF::Annotation::Link> >("links");

  newPageTest("base14-fonts", 0) << QList<QtPDF::Annotation::Link>();

  {
    QList<QtPDF::Annotation::Link> data;
    QtPDF::Annotation::Link l;
    QRectF r;

    l = QtPDF::Annotation::Link();
    r = QRectF(QPointF(102, 500.89), QPointF(276, 513.89));
    l.setRect(r);
    l.setQuadPoints(QPolygonF(r));
    l.setActionOnActivation(new QtPDF::PDFURIAction(QUrl(QString::fromLatin1("http://www.tug.org/texworks/"))));
    data << l;

    l = QtPDF::Annotation::Link();
    r = QRectF(QPointF(142, 488.89), QPointF(159, 498.89));
    l.setRect(r);
    l.setQuadPoints(QPolygonF(r));
    QtPDF::PDFDestination d(1);
    d.setRect(QRectF(103, 712.89, -1, -1));
    l.setActionOnActivation(new QtPDF::PDFGotoAction(d));
    data << l;

    newPageTest("annotations", 0) << data;
  }

}

void TestQtPDF::page_loadLinks()
{
  QFETCH(pPage, page);
  QFETCH(QList<QtPDF::Annotation::Link>, links);

  QTEST_ASSERT(page);

  QList< QSharedPointer<QtPDF::Annotation::Link> > actual = page->loadLinks();

  QCOMPARE(actual.size(), links.size());

  for (int i = 0; i < actual.size(); ++i) {
    compareLinks(*(actual[i]), links[i]);

    if (QTest::currentTestFailed()) {
      qDebug() << "Actual Link" << (i + 1);
      qDebug() << "  " << actual[i]->quadPoints();
      if (actual[i]->actionOnActivation())
        printAction(*(actual[i]->actionOnActivation()));
      qDebug() << "Expected Link" << (i + 1);
      qDebug() << "  " << links[i].quadPoints();
      if (actual[i]->actionOnActivation())
        printAction(*(links[i].actionOnActivation()));
      break;
    }
  }
}

// static
void TestQtPDF::compareAnnotation(const QtPDF::Annotation::AbstractAnnotation & a, const QtPDF::Annotation::AbstractAnnotation & b)
{
  int pageA = (a.page().toStrongRef() ? a.page().toStrongRef()->pageNum() : -1);
  int pageB = (b.page().toStrongRef() ? b.page().toStrongRef()->pageNum() : -1);

  QCOMPARE(a.type(), b.type());
  QCOMPARE(pageA, pageB);
  QCOMPARE(a.rect(), b.rect());
  QCOMPARE(a.contents(), b.contents());
}

// static
void TestQtPDF::compareAnnotations(const QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> > & a, const QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> > & b)
{
  QCOMPARE(a.size(), b.size());

  for (int i = 0; i < a.size(); ++i) {
    compareAnnotation(*(a[i]), *(b[i]));
    if (QTest::currentTestFailed()) return;
  }
}

// static
void TestQtPDF::printAnnotation(const QtPDF::Annotation::AbstractAnnotation & a)
{
  int page = (a.page().toStrongRef() ? a.page().toStrongRef()->pageNum() : -1);
  qDebug() << "   Type:" << a.type() << "page:" << page << "rect:" << a.rect() << "contents:" << a.contents();
}

#define NEW_ANNOT(TYPE, a, r, l) QtPDF::Annotation::TYPE * a = new QtPDF::Annotation::TYPE(); \
a->setPage(page); \
a->setRect(r); \
l << QSharedPointer<QtPDF::Annotation::AbstractAnnotation>(a);


void TestQtPDF::page_loadAnnotations_data()
{
  QTest::addColumn<pPage>("page");
  QTest::addColumn< QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> > >("annotations");

  newPageTest("base14-fonts", 0) << QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> >();
  {
    QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> > l;
    QWeakPointer<QtPDF::Backend::Page> page = _docs[QString::fromLatin1("annotations")]->page(0);

    {
      NEW_ANNOT(Text, a, QRectF(518.824, 701.148, 14.446, 14.445), l)
      a->setContents(QString::fromLatin1("Comment"));
    }
    { NEW_ANNOT(Highlight, a, QRectF(120.817, 686.702, 0, 0), l) }
    { NEW_ANNOT(Underline, a, QRectF(172.2, 686.702, 0, 0), l) }
    { NEW_ANNOT(Squiggly, a, QRectF(225.843, 686.702, 0, 0), l) }
    { NEW_ANNOT(StrikeOut, a, QRectF(273.329, 686.702, 0, 0), l) }
    {
      NEW_ANNOT(Text, a, QRectF(120.817, 643.364, 113.386, 28.347), l)
      a->setContents(QString::fromLatin1("Free text"));
    }
    {
      NEW_ANNOT(Text, a, QRectF(262.549, 628.918, 113.386, 28.347), l)
      a->setContents(QString::fromLatin1("Free text"));
    }
    {
      NEW_ANNOT(Text, a, QRectF(404.281, 614.473, 113.386, 28.346), l)
      a->setContents(QString::fromLatin1("Free text"));
    }
    // FIXME: annotations.pdf contains more annotations (e.g., line, square,
    // circle) but they are currently not supported by QtPDF
    newPageTest("annotations", 0) << l;
  }
}

void TestQtPDF::page_loadAnnotations()
{
  QFETCH(pPage, page);
  QFETCH(QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> >, annotations);

  QList< QSharedPointer<QtPDF::Annotation::AbstractAnnotation> > actual;

  actual = page->loadAnnotations();
  compareAnnotations(actual, annotations);
  if (QTest::currentTestFailed()) {
    qDebug() << "Actual:";
    for (int i = 0; i < actual.size(); ++i)
      printAnnotation(*(actual[i]));
    qDebug() << "Expected:";
    for (int i = 0; i < annotations.size(); ++i)
      printAnnotation(*(annotations[i]));
  }
}

void TestQtPDF::page_boxes_data()
{
  QTest::addColumn<pPage>("page");
  QTest::addColumn<int>("numBoxes");
  QTest::addColumn<int>("iBox");
  QTest::addColumn<QRectF>("bbox");

  newPageTest("annotations", 1) << 6 << 0 << QRectF(102.884, 124.43037059999996, 20.644181800000013, 20.45768120000001);
}

void TestQtPDF::page_boxes()
{
  QFETCH(pPage, page);
  QFETCH(int, numBoxes);
  QFETCH(int, iBox);
  QFETCH(QRectF, bbox);

  QList< QtPDF::Backend::Page::Box > boxes = page->boxes();

  QCOMPARE(boxes.size(), numBoxes);
  if (iBox >= 0)
    QCOMPARE(boxes[iBox].boundingBox, bbox);
}

void TestQtPDF::page_selectedText_data()
{
  QTest::addColumn<pPage>("page");
  QTest::addColumn< QList<QPolygonF> >("selection");
  QTest::addColumn< QString >("text");

  newPageTest("base14-fonts", 0) << QList<QPolygonF>() << QString();
  newPageTest("base14-fonts", 0) << (QList<QPolygonF>() << QPolygonF(QRectF(0, 105, 400, 10))) << QString::fromLatin1("The quick brown fox jumps over the lazy dog");
  newPageTest("base14-fonts", 0) << (QList<QPolygonF>() << QPolygonF(QRectF(0, 100, 400, 15))) << QString::fromLatin1("Times-Roman\nThe quick brown fox jumps over the lazy dog");
  newPageTest("poppler-data", 0) << (QList<QPolygonF>() << QPolygonF(QRectF(0, 75, 95, 15))) << QString::fromUtf8("香港交易");
}

void TestQtPDF::page_selectedText()
{
  QFETCH(pPage, page);
  QFETCH(QList<QPolygonF>, selection);
  QFETCH(QString, text);

  QCOMPARE(page->selectedText(selection), text);
}

// static
void TestQtPDF::compareSearchResults(const QList<QtPDF::Backend::SearchResult> & actual, const QList<QtPDF::Backend::SearchResult> & expected)
{
  QCOMPARE(actual.size(), expected.size());
  for (int i = 0; i < actual.size(); ++i) {
    QCOMPARE(actual[i].pageNum, expected[i].pageNum);
    QCOMPARE(actual[i].bbox, expected[i].bbox);
  }
}

void TestQtPDF::page_search_data()
{
  QTest::addColumn<pPage>("page");
  QTest::addColumn<QString>("needle");
  QTest::addColumn< QList<QtPDF::Backend::SearchResult> >("results");
  QTest::addColumn<QtPDF::Backend::SearchFlags>("flags");

  {
    QList<QtPDF::Backend::SearchResult> l;
    QtPDF::Backend::SearchResult r;
    r.pageNum = 0;
    r.bbox = QRectF(103, 91.804, 68.664, 10.8);
    l << r;

    newPageTest("base14-fonts", 0) << QString::fromLatin1("Times-Roman") << l << QtPDF::Backend::SearchFlags();
    newPageTest("base14-fonts", 0) << QString::fromLatin1("times-Roman") << QList<QtPDF::Backend::SearchResult>() << QtPDF::Backend::SearchFlags();
    newPageTest("base14-fonts", 0) << QString::fromLatin1("times-Roman") << l << QtPDF::Backend::SearchFlags(QtPDF::Backend::Search_CaseInsensitive);
  }
}

void TestQtPDF::page_search()
{
  QFETCH(pPage, page);
  QFETCH(QString, needle);
  QFETCH(QList<QtPDF::Backend::SearchResult>, results);
  QFETCH(QtPDF::Backend::SearchFlags, flags);

  QList<QtPDF::Backend::SearchResult> actual = page->search(needle, flags);
  compareSearchResults(actual, results);

  if (QTest::currentTestFailed()) {
    qDebug() << "Actual:";
    for (int i = 0; i < actual.size(); ++i)
      qDebug() << "  page:" << (actual[i].pageNum + 1) << "bbox:" << actual[i].bbox;
    qDebug() << "Expected:";
    for (int i = 0; i < results.size(); ++i)
      qDebug() << "  page:" << (results[i].pageNum + 1) << "bbox:" << results[i].bbox;
  }
}

void TestQtPDF::paperSize_data()
{
  QTest::addColumn<QSizeF>("requestSize");
  QTest::addColumn<int>("sizeType");
  QTest::addColumn<QString>("label");
  QTest::addColumn<bool>("landscape");

  QTest::newRow("A4 [mm]") << QSizeF(210, 297) << 0 << QString::fromUtf8("DIN A4 [210 × 297 mm]") << false;
  QTest::newRow("A4 [mm approx]") << QSizeF(209, 298) << 0 << QString::fromUtf8("DIN A4 [210 × 297 mm]") << false;
  QTest::newRow("A4 [mm landscape]") << QSizeF(297, 210) << 0 << QString::fromUtf8("DIN A4 [297 × 210 mm]") << true;
  QTest::newRow("A4 [mm landscape approx]") << QSizeF(295, 212) << 0 << QString::fromUtf8("DIN A4 [297 × 210 mm]") << true;
  QTest::newRow("not A4") << QSizeF(210, 294) << 0 << QString::fromUtf8("210 × 294 mm") << false;

  QTest::newRow("A4 [in]") << QSizeF(8.27, 11.7) << 1 << QString::fromUtf8("DIN A4 [210 × 297 mm]") << false;

  QTest::newRow("A4 [pdf]") << QSizeF(595, 842) << 2 << QString::fromUtf8("DIN A4 [210 × 297 mm]") << false;

  QTest::newRow("Letter") << QSizeF(216, 279) << 0 << QString::fromUtf8("Letter (ANSI A) [8.5 × 11 in]") << false;
}

void TestQtPDF::paperSize()
{
  QFETCH(QSizeF, requestSize);
  QFETCH(int, sizeType);
  QFETCH(QString, label);
  QFETCH(bool, landscape);

  QtPDF::PaperSize ps(QString(), QSizeF(0, 0));

  switch (sizeType) {
  case 0:
  default:
    ps = QtPDF::PaperSize::findForMillimeter(requestSize);
    break;
  case 1:
    ps = QtPDF::PaperSize::findForInch(requestSize);
    break;
  case 2:
    ps = QtPDF::PaperSize::findForPDFSize(requestSize);
    break;
  }

  QCOMPARE(ps.label(), label);
  QCOMPARE(ps.landscape(), landscape);
}

void TestQtPDF::transitions_data()
{
#ifdef Q_OS_MACOS
  // Use longer duration on Mac OS as timing seems flaky there (QTBUG-84998)
  constexpr double duration = 0.2;
#else
  constexpr double duration = 0.05;
#endif
  constexpr int w = 10;
  constexpr int h = 10;
  using SPT = QSharedPointer<QtPDF::Transition::AbstractTransition>;
  QTest::addColumn<QtPDF::Transition::AbstractTransition::Type>("type");
  QTest::addColumn<SPT>("transition");
  QTest::addColumn<double>("duration");
  QTest::addColumn<int>("direction");
  QTest::addColumn<QtPDF::Transition::AbstractTransition::Motion>("motion");
  QTest::addColumn<QImage>("imgStart");
  QTest::addColumn<QImage>("imgEnd");

  QImage imgStart{QSize{w, h}, QImage::Format_ARGB32};
  QImage imgEnd{QSize{w, h}, QImage::Format_ARGB32};

  imgStart.fill(Qt::red);
  imgEnd.fill(Qt::blue);

  QTest::newRow("replace") << QtPDF::Transition::AbstractTransition::Type_Replace << SPT(new QtPDF::Transition::Replace) << duration << -1 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("split-H-in") << QtPDF::Transition::AbstractTransition::Type_Split << SPT(new QtPDF::Transition::Split) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("split-V-in") << QtPDF::Transition::AbstractTransition::Type_Split << SPT(new QtPDF::Transition::Split) << duration << 90 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("split-H-out") << QtPDF::Transition::AbstractTransition::Type_Split << SPT(new QtPDF::Transition::Split) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Outward << imgStart << imgEnd;
  QTest::newRow("split-V-out") << QtPDF::Transition::AbstractTransition::Type_Split << SPT(new QtPDF::Transition::Split) << duration << 90 << QtPDF::Transition::AbstractTransition::Motion_Outward << imgStart << imgEnd;
  QTest::newRow("blinds-H") << QtPDF::Transition::AbstractTransition::Type_Blinds << SPT(new QtPDF::Transition::Blinds) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("blinds-V") << QtPDF::Transition::AbstractTransition::Type_Blinds << SPT(new QtPDF::Transition::Blinds) << duration << 90 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("box-in") << QtPDF::Transition::AbstractTransition::Type_Box << SPT(new QtPDF::Transition::Box) << duration << -1 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("box-out") << QtPDF::Transition::AbstractTransition::Type_Box << SPT(new QtPDF::Transition::Box) << duration << -1 << QtPDF::Transition::AbstractTransition::Motion_Outward << imgStart << imgEnd;
  QTest::newRow("wipe-0") << QtPDF::Transition::AbstractTransition::Type_Wipe << SPT(new QtPDF::Transition::Wipe) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("wipe-90") << QtPDF::Transition::AbstractTransition::Type_Wipe << SPT(new QtPDF::Transition::Wipe) << duration << 90 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("wipe-180") << QtPDF::Transition::AbstractTransition::Type_Wipe << SPT(new QtPDF::Transition::Wipe) << duration << 180 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("wipe-270") << QtPDF::Transition::AbstractTransition::Type_Wipe << SPT(new QtPDF::Transition::Wipe) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("dissolve") << QtPDF::Transition::AbstractTransition::Type_Dissolve << SPT(new QtPDF::Transition::Dissolve) << duration << -1 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("glitter-H") << QtPDF::Transition::AbstractTransition::Type_Glitter << SPT(new QtPDF::Transition::Glitter) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("glitter-V") << QtPDF::Transition::AbstractTransition::Type_Glitter << SPT(new QtPDF::Transition::Glitter) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("glitter-D") << QtPDF::Transition::AbstractTransition::Type_Glitter << SPT(new QtPDF::Transition::Glitter) << duration << 315 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("fly-H-in") << QtPDF::Transition::AbstractTransition::Type_Fly << SPT(new QtPDF::Transition::Fly) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("fly-V-in") << QtPDF::Transition::AbstractTransition::Type_Fly << SPT(new QtPDF::Transition::Fly) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("fly-H-out") << QtPDF::Transition::AbstractTransition::Type_Fly << SPT(new QtPDF::Transition::Fly) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Outward << imgStart << imgEnd;
  QTest::newRow("fly-V-out") << QtPDF::Transition::AbstractTransition::Type_Fly << SPT(new QtPDF::Transition::Fly) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Outward << imgStart << imgEnd;
  QTest::newRow("push-H") << QtPDF::Transition::AbstractTransition::Type_Push << SPT(new QtPDF::Transition::Push) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("push-V") << QtPDF::Transition::AbstractTransition::Type_Push << SPT(new QtPDF::Transition::Push) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("cover-H") << QtPDF::Transition::AbstractTransition::Type_Cover << SPT(new QtPDF::Transition::Cover) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("cover-V") << QtPDF::Transition::AbstractTransition::Type_Cover << SPT(new QtPDF::Transition::Cover) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("uncover-H") << QtPDF::Transition::AbstractTransition::Type_Uncover << SPT(new QtPDF::Transition::Uncover) << duration << 0 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("uncover-V") << QtPDF::Transition::AbstractTransition::Type_Uncover << SPT(new QtPDF::Transition::Uncover) << duration << 270 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
  QTest::newRow("fade") << QtPDF::Transition::AbstractTransition::Type_Fade << SPT(new QtPDF::Transition::Fade) << duration << -1 << QtPDF::Transition::AbstractTransition::Motion_Inward << imgStart << imgEnd;
}

void TestQtPDF::transitions()
{
  QFETCH(QtPDF::Transition::AbstractTransition::Type, type);
  QFETCH(QSharedPointer<QtPDF::Transition::AbstractTransition>, transition);
  QFETCH(double, duration);
  QFETCH(int, direction);
  QFETCH(QtPDF::Transition::AbstractTransition::Motion, motion);
  QFETCH(QImage, imgStart);
  QFETCH(QImage, imgEnd);

  ComparableImage cStart{imgStart};
  ComparableImage cEnd{imgEnd};

  // Test defaults
  QCOMPARE(transition->duration(), 1.0);
  QCOMPARE(transition->direction(), 0);
  QCOMPARE(transition->motion(), QtPDF::Transition::AbstractTransition::Motion_Inward);
  QCOMPARE(transition->isRunning(), false);
  QCOMPARE(transition->isFinished(), false);
  QCOMPARE(transition->getImage().isNull(), true);

  // Set and test values
  transition->setDuration(duration);
  transition->setDirection(direction);
  transition->setMotion(motion);
  QCOMPARE(transition->duration(), duration);
  QCOMPARE(transition->direction(), direction);
  QCOMPARE(transition->motion(), motion);

  // Run animation
  transition->start(imgStart, imgEnd);
  QCOMPARE(transition->isRunning(), true);
  QCOMPARE(transition->isFinished(), false);

  sleep(qRound(0.5 * duration * 1000));
  switch (type) {
    case QtPDF::Transition::AbstractTransition::Type_Replace:
      // Replace directly jumps to the final image
      QVERIFY(ComparableImage(transition->getImage()) == cEnd);
      QCOMPARE(transition->isRunning(), false);
      QCOMPARE(transition->isFinished(), true);
      break;
  case QtPDF::Transition::AbstractTransition::Type_Blinds:
  case QtPDF::Transition::AbstractTransition::Type_Box:
  case QtPDF::Transition::AbstractTransition::Type_Cover:
  case QtPDF::Transition::AbstractTransition::Type_Dissolve:
  case QtPDF::Transition::AbstractTransition::Type_Fade:
  case QtPDF::Transition::AbstractTransition::Type_Fly:
  case QtPDF::Transition::AbstractTransition::Type_Glitter:
  case QtPDF::Transition::AbstractTransition::Type_Push:
  case QtPDF::Transition::AbstractTransition::Type_Split:
  case QtPDF::Transition::AbstractTransition::Type_Uncover:
  case QtPDF::Transition::AbstractTransition::Type_Wipe:
    // Test that we get neither the start nor the end image, i.e., we are
    // "somewhere in the middle"
    // TODO: Potentially test specifics, e.g., that a horizontal swipe is
    // really horizontal
    QVERIFY(ComparableImage(transition->getImage()) != cStart);
    QVERIFY(ComparableImage(transition->getImage()) != cEnd);
    QCOMPARE(transition->isRunning(), true);
    QCOMPARE(transition->isFinished(), false);
    break;
  }

  // Wait slightly longer than 0.5 * duration to ensure t>1 in
  // AbstractTransition::getFracTime()
  sleep(qCeil(0.6 * duration * 1000));

  // Test getImage() before isRunning() as the running state is only updated
  // when getImage() is called
  QVERIFY(ComparableImage(transition->getImage()) == cEnd);
  QCOMPARE(transition->isRunning(), false);
  QCOMPARE(transition->isFinished(), true);

  // Check that the image returned when finished is still the same
  QVERIFY(ComparableImage(transition->getImage()) == cEnd);

  // Reset
  transition->reset();
  QCOMPARE(transition->isRunning(), false);
  QCOMPARE(transition->isFinished(), false);
  // Don't test getImage here - it corresponds to the first frame of the
  // animation, not imgStart
}

void TestQtPDF::pageTile()
{
  QList<QtPDF::Backend::PDFPageTile> tiles;

  tiles.append({1., 1., QRect(0, 0, 1, 1), 0});
  tiles.append({2., 1., QRect(0, 0, 1, 1), 0});
  tiles.append({1., 3., QRect(0, 0, 1, 1), 0});
  tiles.append({1., 1., QRect(4, 0, 1, 1), 0});
  tiles.append({1., 1., QRect(0, 5, 1, 1), 0});
  tiles.append({1., 1., QRect(0, 0, 6, 1), 0});
  tiles.append({1., 1., QRect(0, 0, 1, 7), 0});
  tiles.append({1., 1., QRect(0, 0, 1, 1), 8});

  for (int i = 0; i < tiles.size(); ++i) {
    for (int j = i + 1; j < tiles.size(); ++j) {
      auto t1 = tiles[i];
      auto t2 = tiles[j];
      uint h1{QtPDF::Backend::qHash(t1)};
      uint h2{QtPDF::Backend::qHash(t2)};

      QVERIFY(t1 == t1);
      QVERIFY(t2 == t2);
      QCOMPARE(t1 == t2, false);
      QVERIFY(h1 != h2);

      QCOMPARE(t1 < t1, false);
      QCOMPARE(t2 < t2, false);
      QCOMPARE(t1 < t2 || t2 < t1, true);
    }
  }

#ifdef DEBUG
  QCOMPARE(static_cast<QString>(tiles[0]), QStringLiteral("p0,1x1,r0|0x1|1"));
#endif
}

} // namespace UnitTest

#if defined(STATIC_QT5) && defined(Q_OS_WIN)
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

QTEST_MAIN(UnitTest::TestQtPDF)
//#include "TestQtPDF.moc"
