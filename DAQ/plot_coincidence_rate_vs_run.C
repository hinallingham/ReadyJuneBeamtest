// plot_coincidence_rate_vs_run.C
// Plot overall coincidence rate from EventLoaderMALTA/hCoincidenceRateTrend
// vs run number for all beamcheck_run<N>.root files.
//
// Usage:
//   root -l -b -q 'plot_coincidence_rate_vs_run.C()'
//   root -l -b -q 'plot_coincidence_rate_vs_run.C("/path/to/output", "out.png")'

#include <TFile.h>
#include <TProfile.h>
#include <TGraphErrors.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

void plot_coincidence_rate_vs_run(
    const char* output_dir = "/home/hinata/MALTA2/Ready_June/config/2malta_dut/output",
    const char* outpng     = "/home/hinata/MALTA2/Ready_June/DAQ/coincidence_rate_vs_run.png")
{
    std::vector<double> run_vals, rate_vals, rate_errs;

    TSystemDirectory dir(output_dir, output_dir);
    TList* files = dir.GetListOfFiles();
    if (!files) {
        std::cerr << "[ERROR] Cannot list directory: " << output_dir << std::endl;
        return;
    }

    TIter next(files);
    TSystemFile* sysfile;
    while ((sysfile = (TSystemFile*)next())) {
        const char* fname = sysfile->GetName();

        // Match exactly "beamcheck_run<integer>.root"
        int run_num  = -1;
        int consumed = 0;
        if (sscanf(fname, "beamcheck_run%d.root%n", &run_num, &consumed) < 1) continue;
        if (consumed != (int)strlen(fname)) continue;

        std::string fpath = std::string(output_dir) + "/" + fname;
        TFile* f = TFile::Open(fpath.c_str(), "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "[WARN] Cannot open: " << fpath << " — skipping" << std::endl;
            continue;
        }

        TProfile* h = dynamic_cast<TProfile*>(f->Get("EventLoaderMALTA/hCoincidenceRateTrend"));
        if (!h) {
            std::cerr << "[WARN] hCoincidenceRateTrend not found in " << fpath << " — skipping" << std::endl;
            f->Close();
            continue;
        }

        // Weighted mean over all bins.
        // Each bin content = mean coincidence rate [%] over that event window.
        // Each bin has GetBinEntries(b) fills.
        double total_n = 0.0, total_sum = 0.0;
        for (int b = 1; b <= h->GetNbinsX(); ++b) {
            double n = h->GetBinEntries(b);
            total_n   += n;
            total_sum += h->GetBinContent(b) * n;
        }

        if (total_n <= 0.0) {
            std::cerr << "[WARN] No entries in hCoincidenceRateTrend for run "
                      << run_num << " — skipping" << std::endl;
            f->Close();
            continue;
        }

        double rate = total_sum / total_n;           // [%]
        double p    = rate / 100.0;
        // Binomial standard error [%]
        double err  = (p > 0.0 && p < 1.0)
                    ? 100.0 * std::sqrt(p * (1.0 - p) / total_n)
                    : 0.0;

        run_vals.push_back(static_cast<double>(run_num));
        rate_vals.push_back(rate);
        rate_errs.push_back(err);

        std::cout << "[INFO] Run " << run_num
                  << "  coincidence rate = " << rate << " +/- " << err
                  << " %  (N=" << static_cast<long long>(total_n) << ")" << std::endl;

        f->Close();
    }

    int n = static_cast<int>(run_vals.size());
    if (n == 0) {
        std::cerr << "[ERROR] No valid data points found." << std::endl;
        return;
    }

    // Sort by run number
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b){ return run_vals[a] < run_vals[b]; });

    std::vector<double> x(n), y(n), ex(n, 0.0), ey(n);
    for (int i = 0; i < n; ++i) {
        x[i]  = run_vals[idx[i]];
        y[i]  = rate_vals[idx[i]];
        ey[i] = rate_errs[idx[i]];
    }

    TGraphErrors* gr = new TGraphErrors(n, x.data(), y.data(), ex.data(), ey.data());
    gr->SetTitle(";Run Number;Coincidence Rate [%]");
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

    TCanvas* c = new TCanvas("c_coinc", "Coincidence Rate vs Run", 900, 700);
    c->SetMargin(0.13f, 0.05f, 0.13f, 0.10f);

    gr->Draw("ALP");
    gr->GetXaxis()->SetLimits(100.0, x[n-1] + 5.0);
    gr->GetYaxis()->SetRangeUser(82.0, 100.0);
    gPad->Update();

    TLatex latex;
    latex.SetNDC();
    latex.SetTextFont(42);
    latex.SetTextSize(0.038f);
    latex.DrawLatex(0.13, 0.93, "MALTA2 Telescope — Coincidence Rate vs Run (beamcheck)");

    c->Update();
    c->Print(outpng);
    std::cout << "[DONE] Plot saved to " << outpng << std::endl;

    delete c;
    delete gr;
}
