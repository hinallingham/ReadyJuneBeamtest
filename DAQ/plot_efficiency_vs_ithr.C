// plot_efficiency_vs_ithr.C
// Plot MALTA_1 total efficiency vs Ithr from multiple Corryvreckan Analysis ROOT files.
//
// Input file format (one line per run):
//   ITHR  /path/to/Analysis_runXXX.root
//
// Usage:
//   root -l -b -q 'plot_efficiency_vs_ithr.C("pairs.txt","output.png")'

#include <TFile.h>
#include <TEfficiency.h>
#include <TGraphErrors.h>
#include <TCanvas.h>
#include <TAxis.h>
#include <TStyle.h>
#include <TLegend.h>
#include <TLatex.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

void plot_efficiency_vs_ithr(const char* pairs_file, const char* outpng,
                              const char* dut_name = "MALTA_1") {
    std::ifstream ifs(pairs_file);
    if (!ifs.is_open()) {
        std::cerr << "[ERROR] Cannot open pairs file: " << pairs_file << std::endl;
        return;
    }

    std::vector<double> ithr_vals, eff_vals, eff_ehi, eff_elo;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        int ithr;
        char rootpath[1024];
        if (sscanf(line.c_str(), "%d %s", &ithr, rootpath) != 2) continue;

        TFile* f = TFile::Open(rootpath, "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "[WARN] Cannot open: " << rootpath << " — skipping" << std::endl;
            continue;
        }

        std::string effpath = std::string("AnalysisEfficiency/") + dut_name + "/eTotalEfficiency";
        TEfficiency* te = dynamic_cast<TEfficiency*>(f->Get(effpath.c_str()));
        if (!te) {
            std::cerr << "[WARN] eTotalEfficiency not found in " << rootpath << " — skipping" << std::endl;
            f->Close();
            continue;
        }

        double eff = te->GetEfficiency(1);
        double ehi = te->GetEfficiencyErrorUp(1);
        double elo = te->GetEfficiencyErrorLow(1);

        ithr_vals.push_back(static_cast<double>(ithr));
        eff_vals.push_back(eff);
        eff_ehi.push_back(ehi);
        eff_elo.push_back(elo);

        std::cout << "[INFO] Ithr=" << ithr << "  eff=" << eff
                  << "  +/-(" << ehi << ", " << elo << ")  [" << rootpath << "]" << std::endl;

        f->Close();
    }

    int n = static_cast<int>(ithr_vals.size());
    if (n == 0) {
        std::cerr << "[ERROR] No valid data points found." << std::endl;
        return;
    }

    // Sort by Ithr
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b){ return ithr_vals[a] < ithr_vals[b]; });

    std::vector<double> x(n), y(n), exl(n, 0.), exh(n, 0.), eyl(n), eyh(n);
    for (int i = 0; i < n; ++i) {
        x[i]   = ithr_vals[idx[i]];
        y[i]   = eff_vals[idx[i]];
        eyl[i] = eff_elo[idx[i]];
        eyh[i] = eff_ehi[idx[i]];
    }

    TGraphAsymmErrors* gr = new TGraphAsymmErrors(n,
        x.data(), y.data(),
        exl.data(), exh.data(),
        eyl.data(), eyh.data());

    gr->SetTitle(";Ithr [DAC];Total Efficiency");
    gr->SetMarkerStyle(20);
    gr->SetMarkerSize(1.2);
    gr->SetMarkerColor(kAzure + 1);
    gr->SetLineColor(kAzure + 1);
    gr->SetLineWidth(2);

    gStyle->SetOptStat(0);
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleSize(0.05f, "XYZ");
    gStyle->SetLabelSize(0.04f, "XYZ");
    gStyle->SetPadGridX(true);
    gStyle->SetPadGridY(true);

    TCanvas* c = new TCanvas("c_ithr", "Efficiency vs Ithr", 900, 700);
    c->SetMargin(0.13f, 0.05f, 0.13f, 0.10f);

    gr->GetYaxis()->SetRangeUser(0.0, 1.05);
    gr->Draw("AP");

    TLatex latex;
    latex.SetNDC();
    latex.SetTextFont(42);
    latex.SetTextSize(0.038f);
    latex.DrawLatex(0.13, 0.93, "MALTA2 DUT (MALTA_1) - Total Efficiency vs Ithr");

    c->Update();
    c->Print(outpng);
    std::cout << "[DONE] Plot saved to " << outpng << std::endl;

    delete c;
    delete gr;
}
