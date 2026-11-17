// Unit tests: KD Tree, Quad Tree, Range Tree (checked against brute force),
// Interval Tree, Segment Tree, Trie, Suffix Array, Suffix Tree
#include "support/TestFramework.hpp"
#include "data_structures/KDTree.hpp"
#include "data_structures/QuadTree.hpp"
#include "data_structures/RangeTree.hpp"
#include "data_structures/IntervalTree.hpp"
#include "data_structures/SegmentTree.hpp"
#include "data_structures/Trie.hpp"
#include "data_structures/SuffixArray.hpp"
#include "data_structures/SuffixTree.hpp"
#include "utilities/Geo.hpp"

using namespace nexa;
using namespace nexa::ds;

namespace {
DynamicArray<GeoPoint> ncrPoints(int n) {
    DynamicArray<GeoPoint> pts; unsigned long x = 99;
    for (int i = 0; i < n; ++i) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL; double a = (double)((x >> 11) % 100000) / 100000.0;
        x = x * 6364136223846793005ULL + 1442695040888963407ULL; double b = (double)((x >> 11) % 100000) / 100000.0;
        pts.pushBack(GeoPoint{i + 1, 28.40 + 0.40 * a, 77.00 + 0.50 * b, i % 3});
    }
    return pts;
}
}

TEST(Geo, HaversineKnownDistance) {
    // Connaught Place -> Noida Sector 62 (approx 15.6 km straight line)
    double d = geo::haversineKm(28.6315, 77.2167, 28.6270, 77.3727);
    EXPECT_NEAR(d, 15.2, 0.6);
    EXPECT_NEAR(geo::chordToKm(geo::kmToChord(42.0)), 42.0, 1e-9);
    EXPECT_FALSE(geo::validLatLon(91, 0));
}

TEST(KDTree, MatchesBruteForceNearestAndRadius) {
    auto pts = ncrPoints(400);
    KDTree kd; kd.build(pts);
    double qLat = 28.6, qLon = 77.25;
    auto hits = kd.kNearest(qLat, qLon, 5);
    ASSERT_EQ(hits.size(), 5u);
    // brute force
    double best = 1e18; long bestId = -1;
    for (const auto& p : pts) { double d = geo::haversineKm(qLat, qLon, p.lat, p.lon); if (d < best) { best = d; bestId = p.id; } }
    EXPECT_EQ(hits[0].point.id, bestId);
    EXPECT_NEAR(hits[0].distanceKm, best, 1e-6);
    for (std::size_t i = 1; i < hits.size(); ++i) EXPECT_LE(hits[i - 1].distanceKm, hits[i].distanceKm);
    auto ring = kd.withinRadius(qLat, qLon, 6.0);
    std::size_t brute = 0;
    for (const auto& p : pts) if (geo::haversineKm(qLat, qLon, p.lat, p.lon) <= 6.0) ++brute;
    EXPECT_EQ(ring.size(), brute);
    // filter + remove
    auto onlyTag2 = kd.kNearest(qLat, qLon, 3, [](const GeoPoint& p) { return p.tag == 2; });
    for (const auto& h : onlyTag2) EXPECT_EQ(h.point.tag, 2);
    EXPECT_TRUE(kd.remove(bestId));
    EXPECT_NE(kd.kNearest(qLat, qLon, 1)[0].point.id, bestId);
    std::size_t visited = 0;
    kd.kNearest(qLat, qLon, 1, [](const GeoPoint&) { return true; }, &visited);
    EXPECT_LT(visited, 400u);      // pruning happened
}

TEST(QuadTree, RectAndRadiusMatchBruteForce) {
    auto pts = ncrPoints(500);
    QuadTree qt(GeoRect{28.0, 76.8, 29.0, 77.8});
    for (const auto& p : pts) EXPECT_TRUE(qt.insert(p));
    EXPECT_FALSE(qt.insert(GeoPoint{9999, 10, 10, 0}));    // outside bounds
    GeoRect r{28.5, 77.1, 28.65, 77.3};
    std::size_t brute = 0;
    for (const auto& p : pts) if (r.contains(p.lat, p.lon)) ++brute;
    EXPECT_EQ(qt.queryRect(r).size(), brute);
    std::size_t bruteR = 0;
    for (const auto& p : pts) if (geo::haversineKm(28.6, 77.2, p.lat, p.lon) <= 5) ++bruteR;
    EXPECT_EQ(qt.queryRadius(28.6, 77.2, 5, [](const GeoPoint&) { return true; }).size(), bruteR);
    EXPECT_TRUE(qt.remove(pts[0].id, pts[0].lat, pts[0].lon));
    EXPECT_EQ(qt.size(), 499u);
    EXPECT_GT(qt.leafCells().size(), 10u);
}

TEST(RangeTree, MatchesBruteForceIncludingDuplicates) {
    auto pts = ncrPoints(300);
    pts.pushBack(GeoPoint{1001, pts[5].lat, 77.2, 0});     // duplicate latitude
    pts.pushBack(GeoPoint{1002, pts[5].lat, 77.21, 0});
    RangeTree2D rt; rt.build(pts);
    GeoRect boxes[] = {{28.5, 77.1, 28.7, 77.3}, {28.40, 77.0, 28.80, 77.5}, {pts[5].lat, 77.0, pts[5].lat, 77.5}, {30, 80, 31, 81}};
    for (const auto& b : boxes) {
        std::size_t brute = 0;
        for (const auto& p : pts) if (b.contains(p.lat, p.lon)) ++brute;
        EXPECT_EQ(rt.query(b).size(), brute);
    }
}

TEST(IntervalTree, OverlapAndPointQueries) {
    IntervalTree t;
    t.insert({10, 20, 1}); t.insert({15, 25, 2}); t.insert({30, 40, 3}); t.insert({5, 8, 4});
    EXPECT_TRUE(t.anyOverlap(18, 19));
    EXPECT_FALSE(t.anyOverlap(26, 29));
    EXPECT_EQ(t.allOverlaps(12, 31).size(), 3u);
    EXPECT_EQ(t.pointQuery(16).size(), 2u);
    EXPECT_TRUE(t.remove(2, 15));
    EXPECT_EQ(t.pointQuery(16).size(), 1u);
    EXPECT_THROW(t.insert({5, 1, 9}), std::invalid_argument);
}

TEST(SegmentTree, SumMaxLazy) {
    DynamicArray<double> v{1, 3, 5, 7, 9, 11};
    SegmentTree st; st.build(v);
    EXPECT_NEAR(st.rangeSum(1, 3), 15, 1e-9);
    EXPECT_NEAR(st.rangeMax(0, 5), 11, 1e-9);
    st.rangeAdd(0, 2, 10);
    EXPECT_NEAR(st.rangeSum(0, 5), 66, 1e-9);
    EXPECT_NEAR(st.rangeMax(0, 2), 15, 1e-9);
    st.pointSet(5, 0);
    EXPECT_NEAR(st.rangeMax(3, 5), 9, 1e-9);
    EXPECT_THROW(st.rangeSum(4, 9), std::out_of_range);
}

TEST(Trie, PrefixAutocompleteAndDelete) {
    Trie t;
    t.insert("Ghaziabad", 1); t.insert("ghazipur", 2); t.insert("Gurugram", 3); t.insert("ghaziabad", 4);
    auto m = t.withPrefix("gha");
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m[0].word, std::string("ghaziabad"));
    EXPECT_EQ(m[0].ids.size(), 2u);
    EXPECT_TRUE(t.search("GURUGRAM") != nullptr);
    EXPECT_TRUE(t.remove("gurugram", 3));
    EXPECT_TRUE(t.search("gurugram") == nullptr);
    EXPECT_FALSE(t.startsWith("gu"));
    EXPECT_EQ(t.wordCount(), 2u);
}

TEST(SuffixStructures, ArrayAndTreeAgreeWithNaiveSearch) {
    SuffixArray sa;
    sa.addDocument(11, "Building collapse near Sector 18, people trapped");
    sa.addDocument(12, "Gas leak reported; residents trapped in basement");
    sa.addDocument(13, "Flood water rising");
    sa.build();
    auto occ = sa.find("trapped");
    EXPECT_EQ(occ.size(), 2u);
    bool saw11 = false, saw12 = false;
    for (const auto& o : occ) { if (o.docId == 11) saw11 = true; if (o.docId == 12) saw12 = true; }
    EXPECT_TRUE(saw11 && saw12);
    EXPECT_EQ(sa.find("TRAPPED").size(), 2u);
    EXPECT_EQ(sa.find("earthquake").size(), 0u);
    // suffix array must be sorted
    const auto& s = sa.suffixArray(); const auto& txt = sa.text();
    for (std::size_t i = 1; i < s.size(); ++i) EXPECT_TRUE(txt.compare(s[i - 1], std::string::npos, txt, s[i], std::string::npos) < 0);

    std::string text = "banana bandana";
    SuffixTree st; st.build(text);
    auto ana = st.occurrences("ana");
    std::size_t naive = 0;
    for (std::size_t i = 0; i + 3 <= text.size(); ++i) if (text.compare(i, 3, "ana") == 0) ++naive;
    EXPECT_EQ(ana.size(), naive);
    EXPECT_TRUE(st.contains("band"));
    EXPECT_FALSE(st.contains("bandit"));
}
