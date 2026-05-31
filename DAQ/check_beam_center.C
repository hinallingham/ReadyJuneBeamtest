#include <TFile.h>
#include <TCanvas.h>
#include <TH2D.h>
#include <TH2F.h>
#include <TLine.h>
#include <TMarker.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// MALTA2 Beam Center Check
// Usage: check_beam_center("output/beamcheck_runXXX.root", "output/beamcheck/beamcheck_runXXX.png")
//
// Reads from Corryvreckan beamcheck run (MaskCreator module):
//   MaskCreator/DETECTOR/occupancy  (TH2D, pixel coords, hits/px/event)
//   MaskCreator/DETECTOR/maskmap    (TH2F, 1 = masked pixel)
//
// Computes noise-filtered beam centroid (masked pixels excluded),
// converts to mm offset from sensor center (pitch = 36.4 um/pixel).
//
// WARN : |offset| > 1.5 mm
// FAIL : |offset| > 3.0 mm  -> gSystem->Exit(1)
// ---------------------------------------------------------------------------
void check_beam_center(TString filename, TString output_png) {

    const double WARN_MM  = 1.5;
    const double FAIL_MM  = 3.0;
    const double PITCH_MM = 0.0364;   // 36.4 um/pixel

    // MALTA2 geometry
    const int    N_COL   = 224;
    const int    N_ROW   = 512;
    const double CTR_COL = (N_COL - 1) / 2.0;   // 111.5
    const double CTR_ROW = (N_ROW - 1) / 2.0;   // 255.5

    // Extract run number  (e.g. "output/beamcheck_run001.root" -> "001")
    TString run_str = filename;
    run_str.Remove(0, run_str.Last('/') + 1);
    run_str.ReplaceAll("beamcheck_run", "");
    run_str.ReplaceAll(".root", "");

    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(99);

    TFile* f = TFile::Open(filename);
    if (!f || f->IsZombie()) {
        std::cout << "[BEAM_CHECK][ERROR] Cannot open: " << filename << std::endl;
        gSystem->Exit(1);
        return;
    }

    std::vector<TString> detectors = {"MALTA_0", "MALTA_1", "MALTA_2"};
    bool any_fail = false;

    // -------------------------------------------------------------------------
    // Canvas
    // -------------------------------------------------------------------------
    TCanvas* c = new TCanvas("beam_check", Form("Beam Center Check  Run %s", run_str.Data()), 1800, 720);

    TPad* p_title = new TPad("p_title", "", 0.0, 0.91, 1.0, 1.0);
    p_title->SetFillColor(kGray + 3);
    p_title->SetBorderMode(0);
    p_title->Draw();
    p_title->cd();
    TLatex title_tex;
    title_tex.SetNDC();
    title_tex.SetTextFont(62);
    title_tex.SetTextColor(kWhite);
    title_tex.SetTextSize(0.52);
    title_tex.DrawLatex(0.02, 0.20,
        Form("MALTA2 Beam Center Check   Run %s     |  WARN > %.1f mm   FAIL > %.1f mm",
             run_str.Data(), WARN_MM, FAIL_MM));
    c->cd();

    TPad* pads[3];
    for (int i = 0; i < 3; i++) {
        pads[i] = new TPad(Form("pad%d", i), "", i/3.0, 0.0, (i+1)/3.0, 0.91);
        pads[i]->SetRightMargin(0.15);
        pads[i]->SetLeftMargin(0.13);
        pads[i]->SetTopMargin(0.14);
        pads[i]->SetBottomMargin(0.11);
        pads[i]->Draw();
    }

    // -------------------------------------------------------------------------
    std::cout << "\n================================================================" << std::endl;
    std::cout << "  BEAM CENTER CHECK  --  Run " << run_str << std::endl;
    std::cout << "  MALTA2 : " << N_COL << " x " << N_ROW
              << " px  @  " << PITCH_MM*1000 << " um/px" << std::endl;
    std::cout << "  Noise-filtered centroid (maskmap pixels excluded)" << std::endl;
    std::cout << "  WARN : |offset| > " << WARN_MM << " mm" << std::endl;
    std::cout << "  FAIL : |offset| > " << FAIL_MM << " mm" << std::endl;
    std::cout << "================================================================" << std::endl;

    // -------------------------------------------------------------------------
    for (int i = 0; i < 3; i++) {
        TString det      = detectors[i];
        TString occ_path  = Form("MaskCreator/%s/occupancy", det.Data());
        TString mask_path = Form("MaskCreator/%s/maskmap",   det.Data());

        pads[i]->cd();

        TH2D* h_occ  = (TH2D*)f->Get(occ_path);
        TH2F* h_mask = (TH2F*)f->Get(mask_path);

        if (!h_occ) {
            TLatex missing;
            missing.SetNDC(); missing.SetTextSize(0.06); missing.SetTextColor(kRed);
            missing.DrawLatex(0.1, 0.5, Form("%s: occupancy not found", det.Data()));
            std::cout << "  [SKIP]  " << det << " : not found at " << occ_path << std::endl;
            continue;
        }

        // ------------------------------------------------------------------
        // Noise-filtered centroid: skip pixels flagged in maskmap
        // ------------------------------------------------------------------
        double sum_w  = 0, sum_wx = 0, sum_wy = 0;
        int    n_masked = 0;

        for (int col = 1; col <= h_occ->GetNbinsX(); col++) {
            for (int row = 1; row <= h_occ->GetNbinsY(); row++) {
                if (h_mask && h_mask->GetBinContent(col, row) > 0) {
                    n_masked++;
                    continue;
                }
                double occ = h_occ->GetBinContent(col, row);
                if (occ <= 0) continue;
                double x = h_occ->GetXaxis()->GetBinCenter(col);
                double y = h_occ->GetYaxis()->GetBinCenter(row);
                sum_w  += occ;
                sum_wx += occ * x;
                sum_wy += occ * y;
            }
        }

        double mean_col = (sum_w > 0) ? sum_wx / sum_w : CTR_COL;
        double mean_row = (sum_w > 0) ? sum_wy / sum_w : CTR_ROW;
        double off_col  = (mean_col - CTR_COL) * PITCH_MM;
        double off_row  = (mean_row - CTR_ROW) * PITCH_MM;
        double entries  = h_occ->GetEntries();

        const char* status;
        int          status_color;
        if (std::abs(off_col) > FAIL_MM || std::abs(off_row) > FAIL_MM) {
            status       = "FAIL";
            status_color = kRed;
            any_fail     = true;
        } else if (std::abs(off_col) > WARN_MM || std::abs(off_row) > WARN_MM) {
            status       = "WARN";
            status_color = kOrange + 1;
        } else {
            status       = " OK ";
            status_color = kGreen + 2;
        }

        std::cout << Form("  [%s]  %-8s  dX=%+.2f mm  dY=%+.2f mm  "
                          "(N=%.0f hits, %d masked px)",
                          status, det.Data(), off_col, off_row,
                          entries, n_masked) << std::endl;

        // ------------------------------------------------------------------
        // Build display histogram: zero out masked pixels
        // ------------------------------------------------------------------
        TH2D* h_disp = (TH2D*)h_occ->Clone(Form("h_disp_%d", i));
        h_disp->SetDirectory(0);
        if (h_mask) {
            for (int col = 1; col <= h_disp->GetNbinsX(); col++)
                for (int row = 1; row <= h_disp->GetNbinsY(); row++)
                    if (h_mask->GetBinContent(col, row) > 0)
                        h_disp->SetBinContent(col, row, 0);
        }

        h_disp->GetXaxis()->SetTitle("Column [px]");
        h_disp->GetYaxis()->SetTitle("Row [px]");
        h_disp->GetXaxis()->SetTitleSize(0.055);
        h_disp->GetYaxis()->SetTitleSize(0.055);
        h_disp->GetXaxis()->SetLabelSize(0.048);
        h_disp->GetYaxis()->SetLabelSize(0.048);
        h_disp->Draw("colz");

        // Crosshair at sensor center
        double xmin = h_disp->GetXaxis()->GetXmin();
        double xmax = h_disp->GetXaxis()->GetXmax();
        double ymin = h_disp->GetYaxis()->GetXmin();
        double ymax = h_disp->GetYaxis()->GetXmax();

        TLine* lh = new TLine(xmin, CTR_ROW, xmax, CTR_ROW);
        lh->SetLineColor(kWhite); lh->SetLineWidth(1); lh->SetLineStyle(2); lh->Draw("same");

        TLine* lv = new TLine(CTR_COL, ymin, CTR_COL, ymax);
        lv->SetLineColor(kWhite); lv->SetLineWidth(1); lv->SetLineStyle(2); lv->Draw("same");

        // Noise-filtered centroid marker
        TMarker* mk = new TMarker(mean_col, mean_row, 29);
        mk->SetMarkerColor(status_color);
        mk->SetMarkerSize(3.0);
        mk->Draw("same");

        // Annotations
        TLatex lat;
        lat.SetNDC(); lat.SetTextFont(62); lat.SetTextSize(0.063);
        lat.SetTextColor(kBlack); lat.DrawLatex(0.17, 0.88, Form("%s", det.Data()));
        lat.SetTextColor(status_color); lat.DrawLatex(0.60, 0.88, Form("[%s]", status));
        lat.SetTextFont(42); lat.SetTextSize(0.054); lat.SetTextColor(kBlack);
        lat.DrawLatex(0.17, 0.81, Form("dX = %+.2f mm", off_col));
        lat.DrawLatex(0.17, 0.74, Form("dY = %+.2f mm", off_row));
        lat.DrawLatex(0.17, 0.67, Form("masked = %d px", n_masked));
    }

    std::cout << "================================================================" << std::endl;

    c->SaveAs(output_png);
    std::cout << "  Saved : " << output_png << std::endl;
    std::cout << "================================================================\n" << std::endl;

    f->Close();

    if (any_fail) {
        std::cout << "  RESULT : BEAM_CHECK_FAIL" << std::endl;
        gSystem->Exit(1);
    } else {
        std::cout << "  RESULT : BEAM_CHECK_PASS" << std::endl;
    }
}
